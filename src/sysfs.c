/*
 * sysfs.c
 *
 * Implements access to sysfs cpufreq files
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

/* this will contain the start of the sysfs pathes, e.g., /sys/devices/system/cpu/ */
static char * sysfs_start;

/* will be used during compare */
struct setting_s
{
	int len;
	char * string;
};

/*
 * Tries to locate the sysfs by reading /proc/mounts
 */
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

	/*check whether sysfs dir can be opened */
	DIR * dir = opendir(buffer);
	if ( dir == NULL )
	{
		return EIO;
	}

	if (snprintf(buffer, BUFFER_SIZE,"%s/devices/system/cpu/",current_entry->mnt_dir) == BUFFER_SIZE )
	{
		return ENOMEM;
	}
	/* store sysfs start */
	sysfs_start = strdup(buffer);
	return 0;

}

/*
 * will check whether
 * /sys/devices/system/cpu/(cpu)/cpufreq/scaling_governor is either userspace
 * will check for access to
 * /sys/devices/system/cpu/(cpu)/cpufreq/scaling_setspeed
 * */
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
	fd = open(buffer,O_RDWR);
	return fd;
}

/*
 * will return the max nr from /sys/devices/system/cpu/cpu(nr)
 * will fail on sysfs not accessible
 * */
static int freq_gen_sysfs_get_max_sysfs_entries(   )
{
	static long long int max = -1;
	if ( max != -1 )
	{
		return max;
	}
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
	/* go through all files/folders under /sys/devices/system/cpu */
	while ( ( entry = readdir( dir ) ) != NULL )
	{
		/* should be a directory */
		if ( entry->d_type == DT_DIR )
		{
			if ( strlen( entry->d_name ) < 4 )
				continue;

			/* should start with cpu */
			if ( strncmp(entry->d_name, "cpu", 3 ) == 0 )
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
	if ( max == -1 )
		return -EACCES;
	max = max + 1;
	return max;
}

/* prepares a setting that can be applied */
static freq_gen_setting_t freq_gen_prepare_sysfs_access(long long int target,int turbo)
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


/* free a setting that could be applied */
static void unprepare_sysfs_access(freq_gen_setting_t setting_in)
{
	struct setting_s * setting= (struct setting_s* ) setting_in;
	free(setting->string);
	free(setting);
}

/*
 * apply a prepared setting for a CPU
 * */
static long long int freq_gen_sysfs_get_frequency(freq_gen_single_device_t fp)
{
	char buffer[BUFFER_SIZE];
	int result=read((int)fp,buffer, BUFFER_SIZE);
	if (result < 0)
		return result;
	char *tail;
	long long int frequency=strtoll(buffer,&tail,10);
	/* there should only be the in within the file and a \n */
	if ( buffer + result == tail + 1 )
	{
		return frequency*1000;
	}
	else
	{
		return -EIO;
	}
}

/*
 * apply a prepared setting for a CPU
 * */
static int freq_gen_sysfs_set_frequency(freq_gen_single_device_t fp, freq_gen_setting_t setting_in)
{
	struct setting_s * target= (struct setting_s* ) setting_in;
	int result=pwrite((int)fp,target->string,target->len,0);
	if (result==target->len)
		return 0;
	else
		return EIO;
}

static void freq_gen_sysfs_close_file(int cpu_nr, freq_gen_single_device_t fp)
{
	close(fp);
}

static void ignore(){}

static freq_gen_interface_t sysfs_interface =
{
		.name = "sysfs-entries",
		.init_device = freq_gen_sysfs_init_device,
		.get_num_devices = freq_gen_sysfs_get_max_sysfs_entries,
		.prepare_set_frequency = freq_gen_prepare_sysfs_access,
		.get_frequency = freq_gen_sysfs_get_frequency,
		.set_frequency = freq_gen_sysfs_set_frequency,
		.unprepare_set_frequency = unprepare_sysfs_access,
		.close_device = freq_gen_sysfs_close_file,
        .finalize = ignore
};


static freq_gen_interface_t * freq_gen_init_cpufreq( void )
{
	int ret=freq_gen_sysfs_init();
	if (ret)
	{
		errno=ret;
		return NULL;
	}
	return &sysfs_interface;
}

freq_gen_interface_internal_t freq_gen_sysfs_interface_internal =
{
		.init_cpufreq = freq_gen_init_cpufreq,
		.init_uncorefreq = NULL

};
