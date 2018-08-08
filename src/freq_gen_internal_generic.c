/*
 * freq_gen_internal_uncore.c
 *
 *  Created on: 13.02.2018
 *      Author: rschoene
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
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

static int read_file_long(char* file, long int* result)
{
    char buffer[2048];
    int fd = open(file, O_RDONLY);
    if (fd < 0)
    {
        fprintf(stderr, "Could not open %s",file );
        return 1;
    }
    int read_bytes = read(fd, buffer, 2048);
    close(fd);
    if (read_bytes < 0)
    {
        fprintf(stderr, "Could not read %s",file );
        return 1;
    }
    char* endptr;
    *result = strtol(buffer, &endptr, 10);
    return 0;
}

static int read_file_long_list(char* file, long int** result, int* length)
{
    char buffer[2048];
    int fd = open(file, O_RDONLY);
    if (fd < 0)
    {
        fprintf(stderr, "Could not open %s",file );
        return 1;
    }
    int read_bytes = read(fd, buffer, 2048);
    close(fd);
    /* would need larger buffer */
    if (read_bytes == 2048)
    {
        fprintf(stderr, "Could not read %s (insufficient buffer)",file );
        return 1;
    }
    if (read_bytes < 0)
    {
        fprintf(stderr, "Could not read %s",file );
        return 1;
    }
    int end_of_text = read_bytes - 1;
    *result = NULL;
    *length = 0;
    long int nr_processed = 0;

    char* current_ptr = buffer;
    char* next_ptr = NULL;


    /*as long as strtol returns something valid, either !=0 or 0 with errno==0 */
    while ( 1 )
    {
        errno = 0;
        long int read_cpu = strtol( current_ptr, &next_ptr, 10 );
        if ( next_ptr == current_ptr || errno !=0 )
        {
            fprintf(stderr, "Could not read next CPU: %s",current_ptr );
            free(*result);
            *result = NULL;
            *length = 0;
            return 1;
        }
        switch ( *next_ptr )
        {
        /* add cpu */
        case ',':
        {
            long int* tmp = realloc(*result, ((*length) + 1) * sizeof(**result));
            if (!tmp)
            {
                fprintf(stderr, "Could not realloc for CPUs" );
                free(*result);
                *result = NULL;
                *length = 0;
                return 1;
            }
            *result = tmp;
            tmp[*length] = read_cpu;
            (*length)++;
            /*continue at ',' +1 */
            current_ptr = next_ptr + 1;
            break;
        }
        /* just return on end */
        case '\n': /* fall-through */
        case '\0':
            return 0;
        /* range: read another long int */
        case '-':
        {
            errno = 0;
            current_ptr=next_ptr+1;
            long int end_cpu = strtol( current_ptr, &next_ptr, 10 );
            /* if read error return error */
            if ( next_ptr == current_ptr || errno !=0 )
            {
                fprintf(stderr, "Could not read next CPU(2): %s",current_ptr );
                free(*result);
                *result = NULL;
                *length = 0;
                return 1;
            }
            /* if no read error, add range: realloc, then fill */
            long int* tmp = realloc(*result, ((*length) + ( end_cpu - read_cpu + 1)) * sizeof(**result));

            if (!tmp)
            {
                fprintf(stderr, "Could not realloc for CPUs(2)" );
                free(*result);
                *result = NULL;
                *length = 0;
                return 1;
            }

            for ( long int i = read_cpu; i <= end_cpu ; i ++ )
                tmp[ *length + ( i - read_cpu ) ] = read_cpu ;

            *result = tmp;
            (*length) += end_cpu - read_cpu + 1;

            /*next character should be ',' or '\0' */
            switch ( *next_ptr )
            {
            case '\n': /* fall-through */
            case '\0':
                return 0;
            case ',':
                current_ptr = next_ptr + 1;
                break;
            default:
                fprintf(stderr, "Unexpected cpulist encoding (%s) %s",file, next_ptr );
                free(*result);
                *result = NULL;
                *length = 0;
                return 1;
            }
            break;
        }
        /* unexpected character return error */
        default:
            fprintf(stderr, "Unexpected cpulist encoding(2) (%s) %s",file, next_ptr );
            free(*result);
            *result = NULL;
            *length = 0;
            return 1;
        }
    }
    return 0;
}
static int get_package( char * sysfs_path, int node )
{
    long int* cpus;
    int nr_cpus;
    char filename[2048];
    sprintf(filename, "%s/devices/system/node/node%d/cpulist", sysfs_path, node);
    if (read_file_long_list(filename, &cpus, &nr_cpus))
    {
        fprintf(stderr, "Could not read %s\n",filename);
        return -1;
    }
    if (nr_cpus < 1 )
    {
        fprintf(stderr, "Could not read %s -> no cpus found\n",filename);
        return -1;
    }
    long int package_id;
    sprintf(filename, "%s/devices/system/cpu/cpu%ld/topology/physical_package_id", sysfs_path,
            cpus[0] );
    if (read_file_long(filename, &package_id))
    {
        fprintf(stderr, "Could not read %s",filename);
        free(cpus);
        return -1;
    }
    return package_id;

}

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
        return -errno;

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
        return -ferror(proc_mounts);
    }

    /* reached end of file? */
    if (feof(proc_mounts))
    {
        endmntent(proc_mounts);
        return -EINVAL;
    }

    char buffer[BUFFER_SIZE];
    if (snprintf(buffer, BUFFER_SIZE, "%s/devices/system/node", current_entry->mnt_dir) ==
        BUFFER_SIZE)
    {
        endmntent(proc_mounts);
        return -ENOMEM;
    }
    endmntent(proc_mounts);

    /*check whether sysfs dir can be opened */
    DIR* dir = opendir(buffer);
    if (dir == NULL)
    {
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
                long long int current_node = strtoll(&entry->d_name[4], &end, 10);
                /* should end in an int after node */
                if (end != (entry->d_name + strlen(entry->d_name)))
                    continue;
                long long int current_package = get_package(current_entry->mnt_dir, current_node);
                if (current_package > max)
                    max = current_package;
            }
        }
    }
    closedir(dir);
    nr_uncores = max + 1;
    return nr_uncores;
}
