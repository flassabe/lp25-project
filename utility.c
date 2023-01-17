//
// Created by flassabe on 26/10/22.
//

#include "utility.h"

#include <string.h>
#include <dirent.h>
#include <libgen.h>
#include <unistd.h>

#include "global_defs.h"

/*!
 * @brief cat_path concatenates two file system paths into a result. It adds the separation /  if required.
 * @param prefix first part of the complete path
 * @param suffix second part of the complete path
 * @param full_path resulting path
 * @return pointer to full_path if operation succeeded, NULL else
 */
char *concat_path(char *prefix, char *suffix, char *full_path) {
    if (!prefix || !suffix || !full_path)
        return NULL;

    strcpy(full_path, prefix);
    size_t last_char_pos = strlen(full_path);
    if (full_path[last_char_pos-1] != '/') {
        full_path[last_char_pos] = '/';
        full_path[last_char_pos+1] = '\0';
    }
    strcat(full_path, suffix);

    return full_path;
}

bool directory_exists(char *path) {
    if (!path)
        return false;
    DIR *d = opendir(path);
    if (!d)
        return false;
    closedir(d);
    return true;
}

bool path_to_file_exists(char *path) {
    if (!path)
        return false;
    char path_copy[STR_MAX_LEN];
    strcpy(path_copy, path);
    DIR *d = opendir(dirname(path_copy));
    if (!d)
        return false;
    closedir(d);
    return true;
}

void sync_temporary_files(char *temp_dir) {
    DIR *tmp_dir = opendir(temp_dir);
    if (tmp_dir) {
        fsync(dirfd(tmp_dir));
        closedir(tmp_dir);
    }
}

struct dirent *next_dir(struct dirent *entry, DIR *dir) {
    if (!dir)
        return NULL;

    entry = readdir(dir);

    while (1) {
        if (entry) {
            if (entry->d_type != DT_DIR || strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                entry = readdir(dir);
            else
                return entry;
        } else {
            return NULL;
        }
    }
}