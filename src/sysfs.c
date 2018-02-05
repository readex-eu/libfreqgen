/*
 * sysfs.c
 *
 *  Created on: 26.01.2018
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

static freq_gen_interface_t sysfs_interface;

static char * sysfs_start;

static int freq_gen_sysfs_init( void )
{
	/* check whether the sysfs is mounted */
	FILE * proc_mounts = setmntent ("/proc/mounts", "r");

	if (proc_mounts == NULL)
		return errno;

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
		return ferror( proc_mounts );
	}

	/* reached end of file? */
	if ( feof(proc_mounts) )
	{
		endmntent(proc_mounts);
		return EINVAL;
	}

	char buffer[BUFFER_SIZE];
	if (snprintf(buffer, BUFFER_SIZE,"%s/devices/system/cpu/cpufreq",current_entry->mnt_dir) == BUFFER_SIZE )
	{
		endmntent(proc_mounts);
		return ENOMEM;
	}
	endmntent(proc_mounts);

	DIR * dir = opendir(buffer);
	if ( dir == NULL )
	{
		return EIO;
	}

	if (snprintf(buffer, BUFFER_SIZE,"%s/devices/system/cpu/",current_entry->mnt_dir) == BUFFER_SIZE )
	{
		return ENOMEM;
	}

	sysfs_start = strdup(buffer);
	return 0;

}

static freq_gen_single_device_t freq_gen_sysfs_init_device(int cpu)
{

	char buffer[BUFFER_SIZE];
	int fd;
	if (snprintf(buffer,BUFFER_SIZE,"%scpu%d/cpufreq/scaling_governor",sysfs_start,cpu) == BUFFER_SIZE )
	{
		return -ENOMEM;
	}
	fd = open(buffer,O_RDONLY);
	if ( fd < 0 )
	{
		return -EIO;
	}
	if( read(fd,buffer,BUFFER_SIZE) == BUFFER_SIZE )
	{
		return -ENOMEM;
	}
	close(fd);
	if ( strncmp("userspace",buffer,9) != 0)
	{
		return -EPERM;
	}

	if (snprintf(buffer,BUFFER_SIZE,"%scpu%d/cpufreq/scaling_setspeed",sysfs_start,cpu) == BUFFER_SIZE )
	{
		return -ENOMEM;
	}
	fd = open(buffer,O_WRONLY);
	return fd;
}

static int freq_gen_sysfs_get_max_sysfs_entries(   )
{
	if ( sysfs_start == NULL)
	{
		return -EAGAIN;
	}

	DIR * dir = opendir(sysfs_start);
	if ( dir  == NULL)
	{
		return -EIO;
	}
	struct dirent * entry;

	long long int max = 0;
	while ( ( entry = readdir( dir ) ) != NULL )
	{
		if ( entry->d_type == DT_DIR )
		{
			if ( strlen( entry->d_name ) < 4 )
				continue;

			if ( strncmp(entry->d_name, "cpu", 3 ) == 0 )
			{
				/* first after cpu == numerical digit? */

				char* end;
				long long int current=strtoll(&entry->d_name[3],&end,10);
				if ( end != ( entry->d_name + strlen(entry->d_name) ) )
						continue;
				if ( current > max )
					max = current;
			}
		}
	}
	closedir(dir);
	return max;
}

struct setting_s
{
	int len;
	char * string;
};

static freq_gen_setting_t prepare_sysfs_access(long long int target,int turbo)
{
	char buffer[256];
	sprintf(buffer,"%lli",target/1000);
	struct setting_s * setting= malloc(sizeof(struct setting_s));
	if ( setting == NULL)
	{
		return NULL;
	}
	setting->string=strdup(buffer);
	if ( setting->string == NULL)
	{
		free(setting);
		return NULL;
	}
	setting->len=strlen(buffer);
	return setting;
}

static void unprepare_sysfs_access(freq_gen_setting_t setting_in)
{
	struct setting_s * setting= (struct setting_s* ) setting_in;
	free(setting->string);
	free(setting);
}

static int sysfs_set_frequency(freq_gen_single_device_t fp, freq_gen_setting_t setting_in)
{
	struct setting_s * target= (struct setting_s* ) setting_in;
	int result=pwrite((int)fp,target->string,target->len,0);
	if (result==target->len)
		return 0;
	else
		return EIO;
}

static void sysfs_close_file(int cpu_nr, freq_gen_single_device_t fp)
{
	close(fp);
}

static freq_gen_interface_t sysfs_interface =
{
		.name = "sysfs-entries",
		.init_device = freq_gen_sysfs_init_device,
		.get_num_devices = freq_gen_sysfs_get_max_sysfs_entries,
		.prepare_set_frequency = prepare_sysfs_access,
		.set_frequency = sysfs_set_frequency,
		.unprepare_set_frequency = unprepare_sysfs_access,
		.close_device = sysfs_close_file
};

static freq_gen_interface_t * init_cpufreq( void )
{
	int ret;
	if (ret=freq_gen_sysfs_init())
	{
		errno=ret;
		return NULL;
	}
	return &sysfs_interface;
}

freq_gen_interface_internal_t freq_gen_sysfs_interface_internal =
{
		.init_cpufreq = init_cpufreq,
		.init_uncorefreq = NULL

};
