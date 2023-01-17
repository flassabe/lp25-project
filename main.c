#include <stdio.h>
#include <stdint.h>

#include "global_defs.h"
#include "configuration.h"
#include "fifo_processes.h"
#include "mq_processes.h"
#include "direct_fork.h"
#include "reducers.h"
#include "utility.h"
#include "analysis.h"

#include <sys/msg.h>
#include <sys/select.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <dirent.h>

// tv1 - tv2;
int time_diff(struct timeval *tv1, struct timeval *tv2) {
    return (tv1->tv_sec - tv2->tv_sec) * 1000000 +
            tv1->tv_usec - tv2->tv_usec;
}

void clean(char *temp_dir) {
    char step1_path[STR_MAX_LEN], step2_path[STR_MAX_LEN];
    strcpy(step1_path, temp_dir);
    strcat(step1_path, "/step1_output");
    strcpy(step2_path, temp_dir);
    strcat(step2_path, "/step2_output");
    remove(step1_path);
    remove(step2_path);
}

int main(int argc, char *argv[]) {
    struct timeval tv_start, tv_post_mapper2, tv_end;
    configuration_t config = {
            .data_path = "",
            .temporary_directory = "",
            .output_file = "",
            .is_verbose = false,
            .cpu_core_multiplier = 2,
            .method = "mq",
    };
    make_configuration(&config, argv, argc);
    if (!is_configuration_valid(&config)) {
        printf("Incorrect configuration:\n");
        display_configuration(&config);
        printf("\nExiting\n");
        return -1;
    }
    config.process_count = get_nprocs() * config.cpu_core_multiplier;
//    printf("Running analysis on configuration:\n");
//    display_configuration(&config);
//    printf("\nPlease wait, it can take a while\n\n");

    gettimeofday(&tv_start, NULL);
    // Running the analysis, based on defined method:

    if (strcmp(config.method, "mq") == 0) {
        // Initialization
        int mq = make_message_queue();
        if (mq == -1) {
            printf("Could not create MQ, exiting\n");
            return -1;
        }
        pid_t *my_children = mq_make_processes(&config, mq);

        // Execution
        mq_process_directory(&config, mq, my_children);
        sync_temporary_files(config.temporary_directory);
        char temp_result_name[STR_MAX_LEN];
        concat_path(config.temporary_directory, "step1_output", temp_result_name);
        files_list_reducer(config.data_path, config.temporary_directory, temp_result_name);
        mq_process_files(&config, mq, my_children);
        sync_temporary_files(config.temporary_directory);
        gettimeofday(&tv_post_mapper2, NULL);
        printf("Analysis took %d ms\n", time_diff(&tv_post_mapper2, &tv_start)/1000);
        //printf("%d", time_diff(&tv_post_mapper2, &tv_start)/1000);
        //fflush(stdout);
        files_reducer(config.temporary_directory, config.output_file);
        gettimeofday(&tv_end, NULL);
        printf("Complete time %d ms\n", time_diff(&tv_end, &tv_start)/1000);

        // Clean
        clean(config.temporary_directory);
        close_processes(&config, mq, my_children);
        free(my_children);
        close_message_queue(mq);
    }

    if (strcmp(config.method, "fifo") == 0) {
        make_fifos(config.process_count, "fifo-in-%d");
        make_fifos(config.process_count, "fifo-out-%d");
        pid_t *children = make_processes(config.process_count);
        int *command_fifos = open_fifos(config.process_count, "fifo-in-%d", O_WRONLY);
        int *notify_fifos = open_fifos(config.process_count, "fifo-out-%d", O_RDONLY);
        fifo_process_directory(config.data_path, config.temporary_directory, notify_fifos, command_fifos, config.process_count);
        sync_temporary_files(config.temporary_directory);
        char fifo_temp_result_name[STR_MAX_LEN];
        concat_path(config.temporary_directory, "step1_output", fifo_temp_result_name);
        files_list_reducer(config.data_path, config.temporary_directory, fifo_temp_result_name);
        fifo_process_files(config.data_path, config.temporary_directory, notify_fifos, command_fifos, config.process_count);
        sync_temporary_files(config.temporary_directory);
        gettimeofday(&tv_post_mapper2, NULL);
        //printf("%d", time_diff(&tv_post_mapper2, &tv_start)/1000);
        //fflush(stdout);
        printf("Analysis took %d ms\n", time_diff(&tv_post_mapper2, &tv_start)/1000);
        files_reducer(config.temporary_directory, config.output_file);
        gettimeofday(&tv_end, NULL);
        printf("Complete time %d ms\n", time_diff(&tv_end, &tv_start)/1000);
        clean(config.temporary_directory);
        shutdown_processes(config.process_count, command_fifos);
        close_fifos(config.process_count, command_fifos);
        close_fifos(config.process_count, notify_fifos);
        erase_fifos(config.process_count, "fifo-in-%d");
        erase_fifos(config.process_count, "fifo-out-%d");
    }

    if (strcmp(config.method, "direct") == 0) {
        direct_fork_directories(config.data_path, config.temporary_directory, config.process_count);
        sync_temporary_files(config.temporary_directory);
        char direct_temp_result_name[STR_MAX_LEN];
        concat_path(config.temporary_directory, "step1_output", direct_temp_result_name);
        files_list_reducer(config.data_path, config.temporary_directory, direct_temp_result_name);
        direct_fork_files(config.data_path, config.temporary_directory, config.process_count);
        sync_temporary_files(config.temporary_directory);
        gettimeofday(&tv_post_mapper2, NULL);
        //printf("%d", time_diff(&tv_post_mapper2, &tv_start)/1000);
        //fflush(stdout);
        printf("Analysis took %d ms\n", time_diff(&tv_post_mapper2, &tv_start)/1000);
        char direct_step2_file[STR_MAX_LEN];
        concat_path(config.temporary_directory, "step2_output", direct_step2_file);
        files_reducer(config.temporary_directory, config.output_file);
        gettimeofday(&tv_end, NULL);
        printf("Complete time %d ms\n", time_diff(&tv_end, &tv_start)/1000);
        clean(config.temporary_directory);
    }

    return 0;
}
