//
// Created by flassabe on 14/10/22.
//

#include "configuration.h"

#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "utility.h"

configuration_t *make_configuration(configuration_t *base_configuration, char *argv[], int argc) {
    int opt;
    char cli_data_path[STR_MAX_LEN] = "";
    char cli_output_file[STR_MAX_LEN] = "";
    char cli_temporary_directory[STR_MAX_LEN] = "";
    bool cli_is_verbose = false;
    uint8_t cli_cpu_multiplier = 0;
    while ((opt = getopt(argc, argv, "f:d:vo:n:t:m:")) != -1) {
        switch (opt) {
            case 'd':
                strncpy(cli_data_path, optarg, STR_MAX_LEN-1);
                cli_data_path[STR_MAX_LEN-1] = '\0';
                break;
            case 'v':
                cli_is_verbose = true;
                break;
            case 'o':
                strncpy(cli_output_file, optarg, STR_MAX_LEN-1);
                cli_output_file[STR_MAX_LEN-1] = '\0';
                break;
            case 'n':
                cli_cpu_multiplier = strtoul(optarg, NULL, 10);
                break;
            case 't':
                strncpy(cli_temporary_directory, optarg, STR_MAX_LEN-1);
                cli_temporary_directory[STR_MAX_LEN-1] = '\0';
                break;
            case 'f':
                read_cfg_file(base_configuration, optarg);
                break;
            case 'm':
                strcpy(base_configuration->method, optarg);
                break;
            default:
                printf("Unknown error\n");
        }
    }

    // Apply CLI parameters
    if (strlen(cli_data_path) > 0)
        strcpy(base_configuration->data_path, cli_data_path);
    if (strlen(cli_output_file) > 0)
        strcpy(base_configuration->output_file, cli_output_file);
    if (strlen(cli_temporary_directory) > 0)
        strcpy(base_configuration->temporary_directory, cli_temporary_directory);
    base_configuration->is_verbose |= cli_is_verbose;
    if (cli_cpu_multiplier > 0 && cli_cpu_multiplier < 11)
        base_configuration->cpu_core_multiplier = cli_cpu_multiplier;

    return base_configuration;
}

char *skip_spaces(char *str) {
    while (isspace(*str))
        ++str;
    return str;
}

char *check_equal(char *str) {
    str = skip_spaces(str);
    if (*str != '=')
        return NULL;
    ++str;
    str = skip_spaces(str);
    return str;
}

char *get_word(char *source, char *target) {
    if (isspace(*source))
        return NULL;
    while (!isspace(*source) && *source != '\0' && *source != '=') {
        *target = *source;
        ++target;
        ++source;
    }
    *target = '\0';
    return source;
}

configuration_t *read_cfg_file(configuration_t *base_configuration, char *path_to_cfg_file) {
    if (!base_configuration || !path_to_cfg_file)
        return NULL;
    FILE *f = fopen(path_to_cfg_file, "r");
    if (!f)
        return NULL;
    char buffer[STR_MAX_LEN];
    while (fgets(buffer, STR_MAX_LEN, f)) {
        char key[STR_MAX_LEN];
        char value[STR_MAX_LEN];
        char *ptr = buffer;
        ptr = skip_spaces(ptr);
        ptr = get_word(ptr, key);
        ptr = check_equal(ptr);
        if (!ptr)
            continue;
        get_word(ptr, value);
        if (strncmp(key, "data_path", strlen("data_path")) == 0) {
            strncpy(base_configuration->data_path, value, STR_MAX_LEN-1);
            base_configuration->data_path[STR_MAX_LEN-1] = '\0';
            continue;
        }
        if (strncmp(key, "output_file", strlen("output_file")) == 0) {
            strncpy(base_configuration->output_file, value, STR_MAX_LEN-1);
            base_configuration->output_file[STR_MAX_LEN-1] = '\0';
            continue;
        }
        if (strncmp(key, "temporary_directory", strlen("temporary_directory")) == 0) {
            strncpy(base_configuration->temporary_directory, value, STR_MAX_LEN-1);
            base_configuration->temporary_directory[STR_MAX_LEN-1] = '\0';
            continue;
        }
        if (strncmp(key, "is_verbose", strlen("is_verbose")) == 0) {
            if (strncmp(value, "yes", strlen("yes")) == 0) {
                base_configuration->is_verbose = true;
            } else {
                base_configuration->is_verbose = false;
            }
            continue;
        }
        if (strncmp(key, "cpu_core_multiplier", strlen("cpu_core_multiplier")) == 0) {
            uint8_t cpu_mult = strtoul(value, NULL, 10);
            if (cpu_mult > 0 && cpu_mult < 5)
                base_configuration->cpu_core_multiplier = cpu_mult;
            continue;
        }
        printf("Unknown configuration option %s\n", key);
    }
    fclose(f);
    return base_configuration;
}

void display_configuration(configuration_t *configuration) {
    printf("Current configuration:\n");
    printf("\tData source: %s\n", configuration->data_path);
    printf("\tTemporary directory: %s\n", configuration->temporary_directory);
    printf("\tOutput file: %s\n", configuration->output_file);
    printf("\tVerbose mode is %s\n", configuration->is_verbose?"on":"off");
    printf("\tCPU multiplier is %d\n", configuration->cpu_core_multiplier);
    printf("\tProcess count is %d\n", configuration->process_count);
    printf("End configuration\n");
}

bool is_configuration_valid(configuration_t *configuration) {
    if (!directory_exists(configuration->data_path))
        return false;
    if (!directory_exists(configuration->temporary_directory))
        return false;
    if (!path_to_file_exists(configuration->output_file))
        return false;
    if (strcmp(configuration->method, "mq") && strcmp(configuration->method, "fifo") && strcmp(configuration->method, "direct"))
        return false;
    return true;
}
