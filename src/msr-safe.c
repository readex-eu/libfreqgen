/*
 * msr-safe.c
 *
 *  Created on: 26.01.2018
 *      Author: rschoene
 */


#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include  <fcntl.h>
#include <stdlib.h>
#include <errno.h>

#include "freq_gen_internal.h"


#define STEPPING(eax) (eax & 0xF)
#define MODEL(eax) ((eax >> 4) & 0xF)
#define FAMILY(eax) ((eax >> 8) & 0xF)
#define TYPE(eax) ((eax >> 12) & 0x3)
#define EXT_MODEL(eax) ((eax >> 16) & 0xF)
#define EXT_FAMILY(eax) ((eax >> 20) & 0xFF)

#define IA32_PERF_CTL 0x199
#define UNCORE_RATIO_LIMIT 0x620

static freq_gen_interface_t freq_gen_msr_cpu_interface;
static freq_gen_interface_t freq_gen_msr_uncore_interface;


static inline void cpuid(unsigned int *eax, unsigned int *ebx,
                         unsigned int *ecx, unsigned int *edx)
{
        /* ecx is often an input as well as an output. */
        asm volatile("cpuid"
            : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
            : "0" (*eax), "2" (*ecx));
}

static int is_supported()
{
	char buffer[13];
	unsigned int eax = 0, ebx=0, ecx=0, edx=0;
	cpuid(&eax,&ebx,&ecx,&edx);
	buffer[0]=ebx & 0xFF;
	buffer[1]=(ebx>>8) & 0xFF;
	buffer[2]=(ebx>>16) & 0xFF;
	buffer[3]=(ebx>>24) & 0xFF;
	buffer[4]=edx & 0xFF;
	buffer[5]=(edx>>8) & 0xFF;
	buffer[6]=(edx>>16) & 0xFF;
	buffer[7]=(edx>>24) & 0xFF;
	buffer[8]=ecx & 0xFF;
	buffer[9]=(ecx>>8) & 0xFF;
	buffer[10]=(ecx>>16) & 0xFF;
	buffer[11]=(ecx>>24) & 0xFF;
	buffer[12]='\0';
	if (strcmp(buffer, "GenuineIntel") == 0 )
	{
		eax=1;
		cpuid(&eax,&ebx,&ecx,&edx);
		if ( FAMILY(eax) != 6 )
			return 0;
		switch ((EXT_MODEL(eax) << 4 )+ MODEL(eax))
		{
		/* Sandy Bridge */
		case 0x2a:
			break;
		case 0x2d:
			break;
		/* Ivy Bridge */
		case 0x3a:
			break;
		case 0x3e:
			break;
		/* Haswell */
		case 0x3c:
			break;
		case 0x45:
			break;
		case 0x46:
			break;
		case 0x3f:
			break;
		/* Broadwell*/
		case 0x3d:
			break;
		case 0x47:
			break;
		case 0x56:
			break;
		case 0x4f:
			break;
		/* Skylake*/
		case 0x4e:
			break;
		case 0x5e:
			break;
		/* none of the above */
		default:
			return 0;
		}
		return 1;
	}
	if (strcmp(buffer, "AuthenticAMD") == 0 )
	{
		eax=1;
		cpuid(&eax,&ebx,&ecx,&edx);
		if ( FAMILY(eax) == 0x15 )
			return 1;
	}

	return 0;
}
static int is_supported_uncore()
{
	char buffer[13];
	unsigned int eax = 0, ebx=0, ecx=0, edx=0;
	cpuid(&eax,&ebx,&ecx,&edx);
	buffer[0]=ebx & 0xFF;
	buffer[1]=(ebx>>8) & 0xFF;
	buffer[2]=(ebx>>16) & 0xFF;
	buffer[3]=(ebx>>24) & 0xFF;
	buffer[4]=edx & 0xFF;
	buffer[5]=(edx>>8) & 0xFF;
	buffer[6]=(edx>>16) & 0xFF;
	buffer[7]=(edx>>24) & 0xFF;
	buffer[8]=ecx & 0xFF;
	buffer[9]=(ecx>>8) & 0xFF;
	buffer[10]=(ecx>>16) & 0xFF;
	buffer[11]=(ecx>>24) & 0xFF;
	buffer[12]='\0';
	if (strcmp(buffer, "GenuineIntel") == 0 )
	{
		eax=1;
		cpuid(&eax,&ebx,&ecx,&edx);
		if ( FAMILY(eax) != 6 )
			return 0;
		switch ((EXT_MODEL(eax) << 4 )+ MODEL(eax))
		{
		/* Haswell */
		case 0x3c:
			break;
		case 0x45:
			break;
		case 0x46:
			break;
		case 0x3f:
			break;
		/* Broadwell*/
		case 0x3d:
			break;
		case 0x47:
			break;
		case 0x56:
			break;
		case 0x4f:
			break;
		/* Skylake*/
		case 0x4e:
			break;
		case 0x5e:
			break;
		/* none of the above */
		default:
			return 0;
		}
		return 1;
	}

	return 0;
}

static int freq_gen_msr_get_max_entries(   )
{
	static long long int max = 0;
	if ( max != 0 )
	{
		return max;
	}
	char buffer[BUFFER_SIZE];
	DIR * dir = opendir("/dev/cpu/");
	if ( dir  == NULL)
	{
		return -EIO;
	}
	struct dirent * entry;

	while ( ( entry = readdir( dir ) ) != NULL )
	{
		if ( entry->d_type == DT_DIR )
		{
			/* first after cpu == numerical digit? */

			char* end;
			long long int current=strtoll(entry->d_name,&end,10);
			if ( end != ( entry->d_name + strlen(entry->d_name) ) )
					continue;

			/* check access to msr */
			if (snprintf(buffer,BUFFER_SIZE,"/dev/cpu/%lli/msr",current) == BUFFER_SIZE)
			{
				closedir(dir);
				return -ENOMEM;
			}

			/* can not be accessed? check msr-safe */
			if (access (buffer, W_OK) != 0)
			{

				/* check access to msr */
				if (snprintf(buffer,BUFFER_SIZE,"/dev/cpu/%lli/msr-safe",current) == BUFFER_SIZE)
				{
					closedir(dir);
					return -ENOMEM;
				}
				if (access (buffer, W_OK) != 0)
				{
					continue;
				}
			}

			if ( current > max )
				max = current;
		}
	}
	closedir(dir);
	if ( max == 0 )
		return -EACCES;
	return max;
}

static freq_gen_interface_t * freq_gen_msr_init( void )
{
	int ret = freq_gen_msr_get_max_entries();
	if ( ret < 0 )
		return NULL;
	if (! is_supported())
	{
		errno = EINVAL;
		return NULL;
	}
	return &freq_gen_msr_cpu_interface;
}

static freq_gen_interface_t * freq_gen_msr_init_uncore( void )
{
	int ret = freq_gen_msr_get_max_entries();
	if ( ret < 0 )
		return NULL;
	if (! is_supported_uncore())
	{
		errno = EINVAL;
		return NULL;
	}
	return &freq_gen_msr_uncore_interface;
}


static freq_gen_single_device_t freq_gen_msr_device_init( int cpu_id )
{
	char buffer [BUFFER_SIZE];
	/* get uncore msr */

	if ( snprintf(buffer,BUFFER_SIZE,"/dev/cpu/%d/msr",cpu_id) == BUFFER_SIZE )
		return ENOMEM;

	int fd = open(buffer, O_WRONLY);
	if ( fd < 0 )
	{
		if ( snprintf(buffer,BUFFER_SIZE,"/dev/cpu/%d/msr-safe",cpu_id) == BUFFER_SIZE )
			return ENOMEM;
		fd = open(buffer, O_WRONLY);
		if ( fd < 0 )
		{
			return -errno;
		}
	}
	return fd;
}

static freq_gen_single_device_t  freq_gen_msr_device_init_uncore( int uncore )
{
	char buffer [BUFFER_SIZE];
	char * tail;
	/* get uncore id */


	if ( snprintf(buffer,BUFFER_SIZE,"/sys/devices/system/node/node%d/cpulist",uncore) == BUFFER_SIZE )
		return ENOMEM;

	int fd = open(buffer, O_RDONLY);
	if ( fd < 0 )
	{
		return EIO;
	}
	if ( read(fd,buffer,BUFFER_SIZE) <=0 )
	{
		close(fd);
		return EIO;
	}
	close(fd);
	long cpu = strtol(buffer,&tail,10);
	if (tail == buffer)
	{
		return EIO;
	}

	if ( snprintf(buffer,BUFFER_SIZE,"/dev/cpu/%ld/msr",cpu) == BUFFER_SIZE )
		return ENOMEM;

	fd = open(buffer, O_WRONLY);
	if ( fd < 0 )
	{
		if ( snprintf(buffer,BUFFER_SIZE,"/dev/cpu/%ld/msr-safe",cpu) == BUFFER_SIZE )
			return ENOMEM;
		fd = open(buffer, O_WRONLY);
		if ( fd < 0 )
		{
			return -errno;
		}
	}
	return 0;
}

static freq_gen_setting_t freq_gen_msr_prepare_access(long long target,int turbo)
{
	long long int * setting = malloc(sizeof(long long int));
	*setting=((target)/100000000)<<8;
	return setting;
}

static int freq_gen_msr_set_frequency(freq_gen_single_device_t fp, freq_gen_setting_t setting_in)
{
	int result=pwrite(fp,setting_in,8,IA32_PERF_CTL);

	if (result==8)
		return 0;
	else
		return -result;
}


static freq_gen_setting_t freq_gen_msr_prepare_access_uncore(long long target,int turbo)
{
	long long int * setting = malloc(sizeof(long long int));
	*setting=((target)/100000000)+(((target)/100000000)<<8);
	return setting;
}

static int freq_gen_msr_set_frequency_uncore(freq_gen_single_device_t fp, freq_gen_setting_t setting_in)
{
	int result=pwrite(fp,setting_in,8,UNCORE_RATIO_LIMIT);
	if ( result==8 )
		return 0;
	else
		return EIO;
}

static void freq_gen_msr_unprepare_access(freq_gen_setting_t setting)
{
	free(setting);
}

static void freq_gen_msr_close_file(freq_gen_single_device_t fd, int cpu)
{
	close(fd);
}

static void freq_gen_msr_finalize() {}

static freq_gen_interface_t freq_gen_msr_cpu_interface =
{
		.name = "msrsafe-entries",
		.init_device = freq_gen_msr_device_init,
		.get_num_devices = freq_gen_msr_get_max_entries,
		.prepare_set_frequency = freq_gen_msr_prepare_access,
		.set_frequency = freq_gen_msr_set_frequency,
		.unprepare_set_frequency = freq_gen_msr_unprepare_access,
		.close_device = freq_gen_msr_close_file,
		.finalize=freq_gen_msr_finalize
};

static freq_gen_interface_t freq_gen_msr_uncore_interface =
{
		.name = "msrsafe-entries",
		.init_device = freq_gen_msr_device_init_uncore,
		.get_num_devices = freq_gen_msr_get_max_entries,
		.prepare_set_frequency = freq_gen_msr_prepare_access_uncore,
		.set_frequency = freq_gen_msr_set_frequency_uncore,
		.unprepare_set_frequency = freq_gen_msr_unprepare_access,
		.close_device = freq_gen_msr_close_file,
		.finalize=freq_gen_msr_finalize
};

freq_gen_interface_internal_t freq_gen_msr_interface_internal =
{
		.init_cpufreq = freq_gen_msr_init,
		.init_uncorefreq = freq_gen_msr_init_uncore
};
