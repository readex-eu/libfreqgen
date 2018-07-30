/*
 * freq_gen_internal_uncore.c
 *
 *  Created on: 13.02.2018
 *      Author: rschoene
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <mntent.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "freq_gen_internal.h"
#include "../include/error.h"

/*
 * will return the max nr from /sys/devices/system/node/node(nr)
 * will fail on sysfs not accessible
 * */
int freq_gen_get_num_uncore()
{
    static int nr_uncores = 0;

    if (nr_uncores > 0)
    {
        return nr_uncores;
    }
    /* check whether the sysfs is mounted */
    FILE* proc_mounts = setmntent("/proc/mounts", "r");

    if (proc_mounts == NULL)
    {
    	LIBFREQGEN_SET_ERROR("could not access list of mounts in \"/proc/mounts\" while checking if sysfs is mounted");
        return -errno;
    }

    struct mntent* current_entry = getmntent(proc_mounts);
    while (current_entry != NULL)
    {
        if (strcmp(current_entry->mnt_fsname, "sysfs") == 0)
            break;
        current_entry = getmntent(proc_mounts);
    }
    /* reached error? */
    if (ferror(proc_mounts))
    {
        endmntent(proc_mounts);
        LIBFREQGEN_SET_ERROR("I/O-Error when reading \"/proc/mounts\" while checking if sysfs is mounted");
        return -ferror(proc_mounts);
    }

    /* reached end of file? */
    if (feof(proc_mounts))
    {
        endmntent(proc_mounts);
        LIBFREQGEN_SET_ERROR("according to \"/proc/mounts\" the required sysfs is not mounted");
        return -EINVAL;
    }

    char buffer[BUFFER_SIZE];
    if (snprintf(buffer, BUFFER_SIZE, "%s/devices/system/node", current_entry->mnt_dir) ==
        BUFFER_SIZE)
    {
        endmntent(proc_mounts);
        LIBFREQGEN_SET_ERROR("sysfs mount string is too long. Exceeded BUFFER_SIZE (%d)", BUFFER_SIZE);
        return -ENOMEM;
    }
    endmntent(proc_mounts);

    /*check whether sysfs dir can be opened */
    DIR* dir = opendir(buffer);
    if (dir == NULL)
    {
    	LIBFREQGEN_SET_ERROR("could not opendir \"%s\"", buffer);
        return -EIO;
    }

    struct dirent* entry;

    long long int max = -EPERM;
    /* go through all files/folders under /sys/devices/system/node */
    while ((entry = readdir(dir)) != NULL)
    {
        /* should be a directory */
        if (entry->d_type == DT_DIR)
        {
            if (strlen(entry->d_name) < 4)
                continue;

            /* should start with node */
            if (strncmp(entry->d_name, "node", 4) == 0)
            {
                /* first after node == numerical digit? */

                char* end;
                long long int current = strtoll(&entry->d_name[4], &end, 10);
                /* should end in an int after node */
                if (end != (entry->d_name + strlen(entry->d_name)))
                    continue;
                if (current > max)
                    max = current;
            }
        }
    }
    closedir(dir);
    nr_uncores = max + 1;
    return nr_uncores;
}
