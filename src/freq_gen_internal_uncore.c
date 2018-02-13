/*
 * freq_gen_internal_uncore.c
 *
 *  Created on: 13.02.2018
 *      Author: rschoene
 */


#include <stddef.h>
#include <mntent.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include  <fcntl.h>
#include <stdlib.h>
#include <errno.h>

#include "freq_gen_internal.h"

/*
 * will return the max nr from /sys/devices/system/node/node(nr)
 * will fail on sysfs not accessible
 * */
int freq_gen_get_num_uncore( )
{
    static int nr_uncores = 0;

    if ( nr_uncores > 0 )
    {
    	return nr_uncores;
    }
	/* check whether the sysfs is mounted */
	FILE * proc_mounts = setmntent ("/proc/mounts", "r");

	if (proc_mounts == NULL)
		return -errno;

	struct mntent * current_entry = getmntent(proc_mounts);
	while (current_entry != NULL)
	{
		if ( strcmp(current_entry->mnt_fsname,"sysfs") == 0)
			break;
		current_entry = getmntent(proc_mounts);
	}
	/* reached error? */
	if ( ferror( proc_mounts ) )
	{
		endmntent(proc_mounts);
		return -ferror( proc_mounts );
	}

	/* reached end of file? */
	if ( feof(proc_mounts) )
	{
		endmntent(proc_mounts);
		return -EINVAL;
	}

	char buffer[BUFFER_SIZE];
	if (snprintf(buffer, BUFFER_SIZE,"%s/devices/system/node",current_entry->mnt_dir) == BUFFER_SIZE )
	{
		endmntent(proc_mounts);
		return -ENOMEM;
	}
	endmntent(proc_mounts);

	/*check whether sysfs dir can be opened */
	DIR * dir = opendir(buffer);
	if ( dir == NULL )
	{
		return -EIO;
	}

	struct dirent * entry;

	long long int max = -EPERM;
	/* go through all files/folders under /sys/devices/system/cpu */
	while ( ( entry = readdir( dir ) ) != NULL )
	{
		/* should be a directory */
		if ( entry->d_type == DT_DIR )
		{
			if ( strlen( entry->d_name ) < 4 )
				continue;

			/* should start with cpu */
			if ( strncmp(entry->d_name, "node", 3 ) == 0 )
			{
				/* first after cpu == numerical digit? */

				char* end;
				long long int current=strtoll(&entry->d_name[3],&end,10);
				/* should end in an int after cpu */
				if ( end != ( entry->d_name + strlen(entry->d_name) ) )
						continue;
				if ( current > max )
					max = current;
			}
		}
	}
	closedir(dir);
	nr_uncores = max;
	return max;
}
