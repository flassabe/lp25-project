//
// Created by flassabe on 27/10/22.
//

#include "fifo_processes.h"

#include "global_defs.h"
#include <malloc.h>
#include <sys/stat.h>
#include <signal.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>

#include "analysis.h"
#include "utility.h"

void make_fifos(uint16_t processes_count, char *file_format) {
    for (uint16_t i=0; i<processes_count; ++i) {
        char fifo_name[STR_MAX_LEN];
        sprintf(fifo_name, file_format, i);
        if (mkfifo(fifo_name, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) != 0) {
            printf("Error when creating FIFO file %s\n", fifo_name);
        }
    }
}

void erase_fifos(uint16_t processes_count, char *file_format) {
    for (uint16_t i=0; i<processes_count; ++i) {
        char fifo_name[STR_MAX_LEN];
        sprintf(fifo_name, file_format, i);
        if (remove(fifo_name) != 0) {
            printf("Error when deleting FIFO file %s\n", fifo_name);
        }
    }
}

pid_t *make_processes(uint16_t processes_count) {
    pid_t *pid_array = malloc(sizeof(pid_t) * processes_count);
    for (uint16_t i=0; i<processes_count; ++i) {
        pid_t child_pid;
        if ((child_pid = fork()) != 0) { // Parent
            pid_array[i] = child_pid;
        } else { // Child
            free(pid_array);
            pid_t my_pid = getpid();
            printf("Process #%d started\n", my_pid);
            char fifo_name[STR_MAX_LEN];
            volatile sig_atomic_t must_stop = 0;
            sprintf(fifo_name, "fifo-in-%d", i);
            FILE *input_pipe = fopen(fifo_name, "rb");
            sprintf(fifo_name, "fifo-out-%d", i);
            FILE *output_pipe = fopen(fifo_name, "wb");
            if (!input_pipe || !output_pipe) {
                printf("Could not open FIFO %s\n", fifo_name);
                exit(-1);
            }
            int input_fds = fileno(input_pipe);
            int output_fd = fileno(output_pipe);
            while (!must_stop) {
                uint8_t buffer[8+2*STR_MAX_LEN];
                size_t bytes = read(input_fds, buffer, sizeof(buffer));
                if (bytes > 1) {
                    //printf("Process %d received data\n", getpid());
                    task_t *task = (task_t *) buffer;
                    if (task->task_callback == NULL) {
                        printf("PID #%d received STOP command\n", my_pid);
                        must_stop = 1;
                    } else {
                        task->task_callback(task);
                        size_t bytes_sent = write(output_fd, &my_pid, sizeof(pid_t));
                        if (bytes_sent != sizeof (pid_t)) {
                            perror("Error in process sending validation\n");
                            must_stop = 1;
                        }
                    }
                }
            }
            fclose(input_pipe);
            fclose(output_pipe);
            printf("Closing worker with PID #%d\n", getpid());
            exit(0);
        }
    }
    return pid_array;
}

int *open_fifos(uint16_t processes_count, char *file_format, int flags) {
    if (!processes_count || !file_format)
        return NULL;

    int *ret = malloc(sizeof(int) * processes_count);
    for (uint16_t i=0; i<processes_count; ++i) {
        char filename[STR_MAX_LEN];
        sprintf(filename, file_format, i);
        ret[i] = open(filename, flags);
    }

    return ret;
}

void close_fifos(uint16_t processes_count, int *files) {
    if (!files)
        return;

    for (uint16_t i=0; i<processes_count; ++i)
        close(files[i]);
}

void shutdown_processes(uint16_t processes_count, int *fifos) {
    for (uint16_t i=0; i<processes_count; ++i) {
        task_t stop_task = {
                .task_callback = NULL,
                .argument = {0},
        };
        printf("Send STOP to process\n");
        size_t bytes = write(fifos[i], &stop_task, sizeof (task_t));
        if (bytes == 0)
            printf("Error writing\n");
        wait(NULL);
    }
}

int prepare_select(fd_set *fds, const int *filesdes, uint16_t nb_proc) {
    FD_ZERO(fds);
    int max_fd = 0;
    for (uint16_t i=0; i<nb_proc; ++i) {
        max_fd = (max_fd < filesdes[i]) ? filesdes[i] : max_fd;
        FD_SET(filesdes[i], fds);
    }
    return max_fd;
}

void send_task(char *data_source, char *temp_files, char *dir_name, int command_fd) {
    directory_task_t dir_task = {
            .task_callback = process_directory,
    };
    concat_path(data_source, dir_name, dir_task.object_directory);
    strcpy(dir_task.temporary_directory, temp_files);
    size_t bytes = write(command_fd, &dir_task, sizeof (directory_task_t));
    if (bytes != sizeof (directory_task_t)) {
        printf("Send command failed\n");
    }
}

void fifo_process_directory(char *data_source, char *temp_files, int *notify_fifos, int *command_fifos, uint16_t nb_proc) {
    uint16_t current_processes = 0;
    if (!data_source || !temp_files || nb_proc == 0)
        return;

    DIR *dir = opendir(data_source);
    if (!dir)
        return;

    struct dirent *entry = NULL;
    for (uint16_t i=0; i<nb_proc; ++i) { // Fill nb_proc workers with tasks
        entry = next_dir(entry, dir);
        if (entry) {
            send_task(data_source, temp_files, entry->d_name, command_fifos[i]);
            ++current_processes;
        } else // Not enough tasks to fill nb_proc workers
            break;
    }

    entry = next_dir(entry, dir);
    while (entry) {
        if (current_processes >= nb_proc) {
            fd_set read_fds;
            int max_fd = prepare_select(&read_fds, notify_fifos, nb_proc) + 1;
            while (select(max_fd, &read_fds, NULL, NULL, NULL) <= 0);
            for (uint16_t i=0; i<nb_proc; ++i) {
                if (FD_ISSET(notify_fifos[i], &read_fds)) {
                    char buffer[STR_MAX_LEN];
                    read(notify_fifos[i], buffer, STR_MAX_LEN); // Consume data
                    if (entry) {
                        send_task(data_source, temp_files, entry->d_name, command_fifos[i]);
                        entry = next_dir(entry, dir);
                    } else {
                        --current_processes;
                    }
                }
            }
        }
    }

    while (current_processes) {
        fd_set read_fds;
        int max_fd = prepare_select(&read_fds, notify_fifos, nb_proc) + 1;
        while (select(max_fd, &read_fds, NULL, NULL, NULL) <= 0);
        for (uint16_t i=0; i<nb_proc; ++i) {
            if (FD_ISSET(notify_fifos[i], &read_fds)) {
                char buffer[STR_MAX_LEN];
                read(notify_fifos[i], buffer, STR_MAX_LEN); // Consume data
                --current_processes;
                if (current_processes == 0)
                    break;
            }
        }
    }
}

void fifo_process_files(char *data_source, char *temp_files, int *notify_fifos, int *command_fifos, uint16_t nb_proc) {
    uint16_t current_processes = 0;
    if (!data_source || !temp_files || nb_proc == 0)
        return;

    char files_list_name[STR_MAX_LEN];
    char *complete_name = concat_path(temp_files, "step1_output", files_list_name);
    if (!complete_name)
        return;
    FILE *files_list = fopen(files_list_name, "r");
    if (!files_list)
        return;

    file_task_t file_task = {
            .task_callback = process_file,
    };
    strcpy(file_task.temporary_directory, temp_files);

    char line_buffer[STR_MAX_LEN];
    for (uint16_t i=0; i<nb_proc; ++i) { // Fill nb_proc workers with tasks
        if (fgets(line_buffer, STR_MAX_LEN, files_list)) {
            line_buffer[strlen(line_buffer)-1] = '\0'; // Remove trailing \n
            strcpy(file_task.object_file, line_buffer);
            write(command_fifos[i], &file_task, sizeof(file_task_t)); // Should check return
            ++current_processes;
        } else // Not enough tasks to fill nb_proc workers
            break;
    }

    while (1) {
        if (current_processes >= nb_proc) {
            fd_set read_fds;
            int max_fd = prepare_select(&read_fds, notify_fifos, nb_proc) + 1;
            while (select(max_fd, &read_fds, NULL, NULL, NULL) <= 0);
            bool has_no_task_left = false;
            for (uint16_t i=0; i<nb_proc; ++i) {
                if (FD_ISSET(notify_fifos[i], &read_fds)) {
                    char buffer[STR_MAX_LEN];
                    read(notify_fifos[i], buffer, STR_MAX_LEN); // Consume data
                    if (fgets(line_buffer, STR_MAX_LEN, files_list)) {
                        line_buffer[strlen(line_buffer)-1] = '\0'; // Remove trailing \n
                        strcpy(file_task.object_file, line_buffer);
                        write(command_fifos[i], &file_task, sizeof(file_task_t)); // Should check return
                    } else {
                        has_no_task_left = true;
                        --current_processes;
                    }
                }
            }
            if (has_no_task_left)
                break;
        }
    }

    while (current_processes) {
        fd_set read_fds;
        int max_fd = prepare_select(&read_fds, notify_fifos, nb_proc) + 1;
        while (select(max_fd, &read_fds, NULL, NULL, NULL) <= 0);
        for (uint16_t i=0; i<nb_proc; ++i) {
            if (FD_ISSET(notify_fifos[i], &read_fds)) {
                char buffer[STR_MAX_LEN];
                read(notify_fifos[i], buffer, STR_MAX_LEN); // Consume data
                --current_processes;
                if (current_processes == 0)
                    break;
            }
        }
    }
}