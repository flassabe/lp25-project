//
// Created by flassabe on 26/10/22.
//

#include "direct_fork.h"

#include <dirent.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>

#include "analysis.h"
#include "utility.h"

void direct_fork_directories(char *data_source, char *temp_files, uint16_t nb_proc) {
    uint16_t current_processes = 0;
    if (!data_source || !temp_files || nb_proc == 0)
        return;

    DIR *dir = opendir(data_source);
    if (!dir)
        return;

    struct dirent *entry;
    while((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            directory_task_t dir_task = {
                    .task_callback = process_directory,
            };
            concat_path(data_source, entry->d_name, dir_task.object_directory);
            strcpy(dir_task.temporary_directory, temp_files);

            if (current_processes >= nb_proc) {
                wait(NULL);
                --current_processes;
            }

            task_t *task = (task_t *) &dir_task;
            if (fork()) {
                ++current_processes;
            } else {
                closedir(dir);
                task->task_callback(task);
                exit(0);
            }
        }
    }
    while (current_processes > 0) {
        wait(NULL);
        --current_processes;
    }
}

void direct_fork_files(char *data_source, char *temp_files, uint16_t nb_proc) {
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

    char file_line[STR_MAX_LEN];
    while(fgets(file_line, STR_MAX_LEN, files_list)) {
        file_line[strlen(file_line)-1] = '\0'; // Remove trailing \n
        strcpy(file_task.object_file, file_line);

        if (current_processes >= nb_proc) {
            wait(NULL);
            --current_processes;
        }

        task_t *task = (task_t *) &file_task;
        if (fork()) {
            ++current_processes;
        } else {
            fclose(files_list);
            task->task_callback(task);
            exit(0);
        }
    }
    while (current_processes > 0) {
        wait(NULL);
        --current_processes;
    }
}