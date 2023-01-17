//
// Created by flassabe on 26/10/22.
//

#include "analysis.h"

#include <dirent.h>
#include <stddef.h>
#include <unistd.h>
#include <libgen.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/file.h>

#include "utility.h"

void parse_dir(char *path, FILE *output_file) {
    if (!output_file || !path)
        return;

    DIR *dir = opendir(path);
    if (!dir)
        return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            fprintf(output_file, "%s/%s\n", path, entry->d_name);
        } else if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") !=0 && strcmp(entry->d_name, "..") != 0) {
            char new_path[STR_MAX_LEN];
            if (concat_path(path, entry->d_name, new_path)) {
                parse_dir(new_path, output_file);
            }
        }
    }
    closedir(dir);
}

typedef struct _recipient {
    char email[STR_MAX_LEN];
    struct _recipient *next;
} simple_recipient_t;

void clear_recipient_list(simple_recipient_t *list) {
    while (list) {
        simple_recipient_t *tmp = list;
        list = list->next;
        free(tmp);
    }
}

simple_recipient_t *add_recipient_to_list(char *recipient_email, simple_recipient_t *list) {
    if (!recipient_email)
        return list;

    simple_recipient_t *new_recipient = malloc(sizeof (simple_recipient_t));
    if (!new_recipient)
        return list;

    new_recipient->next = list;
    strcpy(new_recipient->email, recipient_email);
    return new_recipient;
}

bool is_address_valid(char *address) {
    bool at_read = false;
    for (size_t i=0; i<strlen(address); ++i) {
        if (address[i] == '<' || address[i] == '>' || address[i] == '=')
            return false; // Simplified processing. We shall strip <> chars to extract the real address
        if (address[i] == '@') {
            if (!at_read)
                at_read = true;
            else
                return false; // No double @ in e-mail
        }
    }
    if (at_read)
        return true;
    return false;
}

simple_recipient_t *extract_emails(char *buffer, simple_recipient_t *list) {
    if (!buffer)
        return list;
    char *address = strtok(buffer, ", \t");
    while (address) {
        if (is_address_valid(address))
            list = add_recipient_to_list(address, list);
        address = strtok(NULL, ", \t");
    }
    return list;
}

void extract_e_mail(char buffer[], char destination[]) {
    char *character = buffer;
    char *wr = destination;
    while (isspace(*character)) ++character;
    while (*character != ' ' && *character != '\r' && *character != '\n' && *character != '\0' && *character != '\t') {
        *wr++ = *character++;
    }
    *wr = '\0';
}

typedef enum {IN_DEST_FIELD, OUT_OF_DEST_FIELD} read_status_t;

void parse_file(char *filepath, char *output) {
    if (!filepath || !output)
        return;

    FILE *mail_file = fopen(filepath, "r");
    if (!mail_file)
        return;

    char sender_email[STR_MAX_LEN] = "";
    simple_recipient_t *recipient_list = NULL;
    read_status_t status = OUT_OF_DEST_FIELD;
    char read_buffer[5*STR_MAX_LEN];
    while(fgets(read_buffer, sizeof (read_buffer)-1, mail_file)) {
        char *last_char = &read_buffer[strlen(read_buffer)-1];
        while (*last_char == '\n' || *last_char == '\r' || *last_char == ' ') { // strip trailing EOL and spaces
            *last_char = '\0';
            if (last_char != read_buffer)
                --last_char;
            else
                break;
        }
        if (strncmp(read_buffer, "From: ", strlen("From: ")) == 0) {
            extract_e_mail(&read_buffer[6], sender_email);
            if (!is_address_valid(sender_email))
                break;
            //printf("From address: %s\n", sender_email);
        } else if (strncmp(read_buffer, "To: ", strlen("To: ")) == 0
        || strncmp(read_buffer, "Cc: ", strlen("Cc: ")) == 0) {
            recipient_list = extract_emails(&read_buffer[4], recipient_list);
            if (*last_char == ',')
                status = IN_DEST_FIELD;
        } else if (strncmp(read_buffer, "Bcc: ", strlen("Bcc: ")) == 0) {
            recipient_list = extract_emails(&read_buffer[5], recipient_list);
            if (*last_char == ',')
                status = IN_DEST_FIELD;
        } else if (status == IN_DEST_FIELD) {
            recipient_list = extract_emails(read_buffer, recipient_list);
            if (*last_char != ',')
                status = OUT_OF_DEST_FIELD;
        } else if (strncmp(read_buffer, "X-From: ", strlen("X-From: ")) == 0) // If X-From is read, we already had all other required fields
            break;
    }
    fclose(mail_file);

    if (strlen(sender_email) == 0 || !recipient_list) {
        clear_recipient_list(recipient_list);
        return;
    }

    int output_file = open(output, O_RDWR | O_APPEND | O_CREAT, 00660);
    if (output_file != -1) {
        // Count elements in list
        uint32_t elements_count = 2 + strlen(sender_email); // init to sender e-mail length + trailing \n + \0
        simple_recipient_t *p = recipient_list;
        while (p) {
            elements_count += 1 + strlen(p->email); // space + length of e-mail
            p = p->next;
        }
        // Make buffer to write
        char *write_buffer = malloc(sizeof(char) * elements_count);
        p = recipient_list;
        strcpy(write_buffer, sender_email);
        while (p) {
            strcat(write_buffer, " ");
            strcat(write_buffer, p->email);
            p = p->next;
        }
        strcat(write_buffer, "\n");
        while (flock(output_file, LOCK_EX) == -1);
        lseek(output_file, 0, SEEK_END);
        write(output_file, write_buffer, elements_count-1); // Don't write \0 into file
        // Delete buffer
        //printf("%s", write_buffer);
        free(write_buffer);
//        fsync(output_file);
        flock(output_file, LOCK_UN);
        close(output_file);
    }

    clear_recipient_list(recipient_list);
}

void process_directory(task_t *task) {
    if (!task)
        return;

    directory_task_t *dir_task = (directory_task_t *) task;
    //printf("Process %d processing directory %s to %s\n", getpid(), dir_task->object_directory, dir_task->temporary_directory);

    // Extract basename of the path
    char copy_of_obj_dir[STR_MAX_LEN];
    strcpy(copy_of_obj_dir, dir_task->object_directory);
    char *base_name = basename(copy_of_obj_dir);

    // Open output file
    char output_file[STR_MAX_LEN];
    concat_path(dir_task->temporary_directory, base_name, output_file);
    FILE *f = fopen(output_file, "w");

    // Call parse_dir on path and opened file
    if (f) {
        parse_dir(dir_task->object_directory, f);
        fclose(f);
    } else {
        printf("Could not open file %s\n", output_file);
    }

    //printf("Process %d finished its task\n", getpid());
}

void process_file(task_t *task) {
    if (!task)
        return;

    file_task_t *file_task = (file_task_t *) task;
    char output_file[STR_MAX_LEN];
    concat_path(file_task->temporary_directory, "step2_output", output_file);
    parse_file(file_task->object_file, output_file);
}
