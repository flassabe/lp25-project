//
// Created by flassabe on 10/11/22.
//

#include "mq_processes.h"

#include <sys/msg.h>

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <bits/types/sig_atomic_t.h>
#include <stdio.h>

#include "utility.h"
#include "analysis.h"

int make_message_queue() {
    key_t my_token = ftok("/bin", 25);
    if (my_token == -1)
        return -1;
    return msgget(my_token, 0666 | IPC_CREAT);
}

void close_message_queue(int mq) {
    if (mq >= 0) {
        msgctl(mq, IPC_RMID, NULL);
    }
}

void child_process(int mq) {
    volatile sig_atomic_t end_loop = 0;
    mq_message_t message;
    pid_t my_pid = getpid();
    while (!end_loop) {
        ssize_t bytes_received = msgrcv(mq, &message, sizeof (message.mtext), my_pid, 0);
        if (bytes_received == sizeof(message.mtext)) {
            task_t *task = (task_t *) message.mtext;
            if (task->task_callback == NULL) {
                end_loop = 1;
                continue;
            }
            task->task_callback(task);
            message.mtype = 1;
            memcpy(message.mtext, &my_pid, sizeof (pid_t));
            if (msgsnd(mq, &message, sizeof (pid_t), 0) != 0)
                end_loop = 1;
        }
    }
}

pid_t *mq_make_processes(configuration_t *config, int mq) {
    if (!config || mq == -1)
        return NULL;

    pid_t *pids_created = malloc(sizeof(pid_t) * config->process_count);
    for (uint16_t i=0; i<config->process_count; ++i) {
        pid_t child_pid = fork();
        if (child_pid == 0) {
            child_process(mq);
            exit(0);
        } else {
            pids_created[i] = child_pid;
        }
    }
    return pids_created;
}

void close_processes(configuration_t *config, int mq, pid_t children[]) {
    if (!config || mq == -1)
        return;

    task_t task = {
            .task_callback = NULL,
    };
    mq_message_t close_message;
    memcpy(close_message.mtext, &task, sizeof (task_t)); // Only once, every child receives the same

    for (uint16_t i=0; i<config->process_count; ++i) {
        close_message.mtype = (long) children[i];
        msgsnd(mq, &close_message, sizeof(task_t), 0);
    }
}

void send_task_to_mq(char data_source[], char temp_files[], char target_dir[], int mq, pid_t worker_pid) {
    mq_message_t command = {
            .mtype = worker_pid,
    };
    directory_task_t *task = (directory_task_t *) command.mtext;
    task->task_callback = process_directory;

    concat_path(data_source, target_dir, task->object_directory);
    strcpy(task->temporary_directory, temp_files);
    if (msgsnd(mq, &command, sizeof(directory_task_t), 0) == -1) {
        printf("Send command failed\n");
    }
}

void send_file_task_to_mq(char data_source[], char temp_files[], char target_file[], int mq, pid_t worker_pid) {
    mq_message_t command = {
            .mtype = worker_pid,
    };
    file_task_t *task = (file_task_t *) command.mtext;
    task->task_callback = process_file;

    strcpy(task->object_file, target_file);
    strcpy(task->temporary_directory, temp_files);
    if (msgsnd(mq, &command, sizeof(file_task_t), 0) == -1) {
        printf("Send command failed\n");
    }
}

void mq_process_directory(configuration_t *config, int mq, pid_t children[]) {
    uint16_t current_processes = 0;

    DIR *dir = opendir(config->data_path);
    if (!dir)
        return;

    struct dirent *entry = next_dir(entry, dir);
    for (uint16_t i=0; i<config->process_count; ++i) {
        if (!entry)
            break;
        send_task_to_mq(config->data_path, config->temporary_directory, entry->d_name, mq, children[i]);
        ++current_processes;
        entry = next_dir(entry, dir);
    }

    while (current_processes > 0) {
        mq_message_t message;
        ssize_t bytes_received = msgrcv(mq, &message, sizeof (pid_t), 1, 0);
        if (bytes_received == sizeof (pid_t)) {
            pid_t available_child_pid;
            memcpy(&available_child_pid, message.mtext, sizeof (pid_t));
            if (entry) {
                send_task_to_mq(config->data_path, config->temporary_directory, entry->d_name, mq, available_child_pid);
                entry = next_dir(entry, dir);
            } else {
                --current_processes;
            }
        } else {
            printf("Malformed message, %ld bytes long\n", bytes_received);
        }
    }
}

void mq_process_files(configuration_t *config, int mq, pid_t children[]) {
    uint16_t current_processes = 0;

    char step1_file[STR_MAX_LEN];
    concat_path(config->temporary_directory, "step1_output", step1_file);
    FILE *f = fopen(step1_file, "r");
    if (!f) {
        printf("No intermediate result was found\n");
        return;
    }

    char read_buffer[STR_MAX_LEN];
    char *result = fgets(read_buffer, STR_MAX_LEN, f);
    for (uint16_t i=0; i<config->process_count; ++i) {
        if (!result)
            break;
        if (read_buffer[strlen(read_buffer)-1] == '\n')
            read_buffer[strlen(read_buffer)-1] = '\0';
        send_file_task_to_mq(config->data_path, config->temporary_directory, read_buffer, mq, children[i]);
        ++current_processes;
        result = fgets(read_buffer, STR_MAX_LEN, f);
    }

    while (current_processes > 0) {
        mq_message_t message;
        ssize_t bytes_received = msgrcv(mq, &message, sizeof (pid_t), 1, 0);
        if (bytes_received == sizeof (pid_t)) {
            pid_t available_child_pid;
            memcpy(&available_child_pid, message.mtext, sizeof (pid_t));
            if (result) {
                if (read_buffer[strlen(read_buffer)-1] == '\n')
                    read_buffer[strlen(read_buffer)-1] = '\0';
                send_file_task_to_mq(config->data_path, config->temporary_directory, read_buffer, mq, available_child_pid);
                result = fgets(read_buffer, STR_MAX_LEN, f);
            } else {
                --current_processes;
            }
        } else {
            printf("Malformed message, %ld bytes long\n", bytes_received);
        }
    }
}
