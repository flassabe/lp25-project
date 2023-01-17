//
// Created by flassabe on 26/10/22.
//

#include "reducers.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>

#include "global_defs.h"
#include "utility.h"
#include "configuration.h"

sender_t *create_sender(char *email) {
    sender_t *new_sender = malloc(sizeof(sender_t));
    new_sender->head = NULL;
    new_sender->tail = NULL;
    new_sender->prev = NULL;
    new_sender->next = NULL;
    strcpy(new_sender->sender_address, email);
    return new_sender;
}

// Change in this function: list is modified, return value is a pointer to the added element, even if not first
sender_t *add_source_to_list(sender_list_t *list, char *source_email) {
    if (!list || !source_email)
        return NULL;

    if (!list->head) {
        sender_t *new_sender = create_sender(source_email);
        list->head = new_sender;
        list->tail = new_sender;
        return new_sender;
    }

    sender_t *ptr = list->head;
    while (ptr) {
        int cmp = strcmp(source_email, ptr->sender_address);
        if (cmp == 0)
            return ptr; // No change
        else if (cmp > 0) {
            if (ptr->next) { // Not the last element
                ptr = ptr->next;
                continue;
            } else { // Last element, add to tail
                ptr->next = create_sender(source_email);
                ptr->next->prev = ptr;
                list->tail = ptr->next;
                return list->tail;
            }
        } else { // New sender is smaller than current, add before
            sender_t *new_sender = create_sender(source_email);
            if (ptr == list->head) { // Add to head
                new_sender->next = list->head;
                list->head->prev = new_sender;
                list->head = new_sender;
                return new_sender;
            } else { // Insert before current element
                sender_t *previous = ptr->prev;
                new_sender->prev = previous;
                new_sender->next = ptr;
                previous->next = new_sender;
                ptr->prev = new_sender;
                return new_sender;
            }
        }
    }
    return NULL;
}

void clear_sources_list(sender_t *list) {
    sender_t *tmp;
    while (list) {
        tmp = list;
        list = list->next;
        recipient_t *tmp_recipient;
        while (tmp->head) {
            tmp_recipient = tmp->head;
            tmp->head = tmp->head->next;
            free(tmp_recipient);
        }
        free(tmp);
    }
}

/* Do not use for ordered list
sender_t *find_source_in_list(sender_t *list, char *source_email) {
    while (list) {
        int cmp = strcmp(list->sender_address, source_email);
        if (cmp == 0)
            return list;
        else if (cmp > 0)
            return NULL;
        list = list->next;
    }
    return NULL;
}
*/

void add_recipient_to_source(sender_t *source, char *recipient_email) {
    if (!source || !recipient_email)
        return;
    recipient_t *ptr = source->head;
    while (ptr) {
        if (strcmp(ptr->recipient_address, recipient_email) == 0) {
            ptr->occurrences++;
            return;
        }
        ptr = ptr->next;
    }
    recipient_t *new_recipient = malloc(sizeof (recipient_t));
    new_recipient->occurrences = 1;
    strcpy(new_recipient->recipient_address, recipient_email);
    new_recipient->next = source->head;
    new_recipient->prev = NULL;
    if (source->head) {
        source->head->prev = new_recipient;
    } else {
        source->tail = new_recipient;
    }
    source->head = new_recipient;
}

void files_list_reducer(char *data_source, char *temp_files, char *output_file) {
    if (!temp_files || !output_file || !data_source)
        return;

    DIR *dir = opendir(data_source);
    if (!dir)
        return;

    FILE *test_file = fopen(output_file, "wb");
    if (!test_file) {
        closedir(dir);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char full_name[STR_MAX_LEN]; // Make file name from dir name (dir is in source, its analysis result is a file in temp)
            concat_path(temp_files, entry->d_name, full_name);
            FILE *source_file = fopen(full_name, "rb");
            if (source_file) {
                size_t bytes_read;
                uint8_t buffer[1024];
                while ((bytes_read = fread(buffer, 1, 1024, source_file)) != 0) {
                    fwrite(buffer, 1, bytes_read, test_file);
                }
                fclose(source_file);
                remove(full_name);
            } else {
                printf("Failed to open file %s\n", full_name);
                printf("\tError %d\n", errno);
            }
        }
    }

    fclose(test_file);
    closedir(dir);
}

void files_reducer(char *temp_files, char *output_file) {
    if (!temp_files || !output_file)
        return;
    char step2_path[STR_MAX_LEN];
    if (concat_path(temp_files, "step2_output", step2_path) != step2_path)
        return;
    FILE *input_file = fopen(step2_path, "r");
    if (!input_file)
        return;
    FILE *output = fopen(output_file, "w");
    if (!output) {
        fclose(input_file);
        return;
    }

#define VERY_LONG STR_MAX_LEN*STR_MAX_LEN
    char buffer[VERY_LONG]; // Let's allocate a veeeery long buffer
    sender_list_t result = {.head=NULL, .tail=NULL};
    sender_t *current_sender = NULL;
    while (fgets(buffer, VERY_LONG, input_file)) {
        buffer[strlen(buffer)-1] = '\0';
        char *cursor = skip_spaces(buffer);
        char sender[STR_MAX_LEN];
        cursor = get_word(cursor, sender);
        if (cursor) {
            current_sender = add_source_to_list(&result, sender);
            if (!current_sender)
                continue;
        } else
            continue;
        while(1) {
            cursor = skip_spaces(cursor);
            if (*cursor == '\0') break;
            char recipient[STR_MAX_LEN];
            cursor = get_word(cursor, recipient);
            if (cursor) {
                add_recipient_to_source(current_sender, recipient);
            }
        }
    }

    // Write output
    sender_t *sender_ptr = result.head;
    while (sender_ptr) {
        fprintf(output, "%s:", sender_ptr->sender_address);
        recipient_t *recipient_ptr = sender_ptr->head;
        while (recipient_ptr) {
            fprintf(output, " %d:%s", recipient_ptr->occurrences, recipient_ptr->recipient_address);
            recipient_ptr = recipient_ptr->next;
        }
        fprintf(output, "\n");
        sender_ptr = sender_ptr->next;
    }
    fclose(output);
    clear_sources_list(result.head);
}