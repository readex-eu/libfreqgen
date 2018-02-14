/*
 * msr-safe.c
 *
 * This implements access to /dev/cpu/(nr)/msr[-safe]
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
#include "freq_gen_internal_uncore.h"

/* some definitions to parse cpuid */
#define STEPPING(eax) (eax & 0xF)
#define MODEL(eax) ((eax >> 4) & 0xF)
#define FAMILY(eax) ((eax >> 8) & 0xF)
#define TYPE(eax) ((eax >> 12) & 0x3)
#define EXT_MODEL(eax) ((eax >> 16) & 0xF)
#define EXT_FAMILY(eax) ((eax >> 20) & 0xFF)

/* used registers for core/uncore frequency toggling */
#define IA32_PERF_STATUS 0x198
#define IA32_PERF_CTL 0x199
#define UNCORE_RATIO_LIMIT 0x620

/* implementations of the interface */
static freq_gen_interface_t freq_gen_msr_cpu_interface;
static freq_gen_interface_t freq_gen_msr_uncore_interface;


/* cpuid call in C */
static inline void cpuid(unsigned int *eax, unsigned int *ebx,
                         unsigned int *ecx, unsigned int *edx)
{
        /* ecx is often an input as well as an output. */
        asm volatile("cpuid"
            : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
            : "0" (*eax), "2" (*ecx));
}

/* check whether frequency scaling for the current CPU is supported
 * returns 0 if not or 1 if so
 */
static int is_supported()
{
	char buffer[13];
	unsigned int eax = 0, ebx=0, ecx=0, edx=0;
	cpuid(&eax,&ebx,&ecx,&edx);

	/* reorder name string */
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
	/* currently only Fam15h */
	if (strcmp(buffer, "AuthenticAMD") == 0 )
	{
		eax=1;
		cpuid(&eax,&ebx,&ecx,&edx);
		if ( FAMILY(eax) == 0x15 )
			return 1;
	}

	return 0;
}

/* check whether uncore frequency scaling for the current CPU is supported
 * returns 0 if not or 1 if so
 */
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


/* this will return the maximal number of CPUs by looking for /dev/cpu/(nr)/msr[-safe]
 * It will also check whether these can be written
 * time complexity is O(num_cpus) for the first call. Afterwards its O(1), since the return value is buffered
 */
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

/* will return the core frequency interface if and only if the CPU is supported and /dev/.../msr[-safe] can be written */
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

/* will return the uncore frequency interface if and only if the CPU is supported and /dev/.../msr[-safe] can be written
 */
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


/* will open a file descriptor for /dev/cpu/(cpu_id)/msr[-safe] and return it.
 * /dev/cpu/(cpu)/msr[-safe] must be writable
 */
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

/* will open a file descriptor to the first cpu of a given uncore /dev/cpu/(cpu)/msr[-safe] and return it.
 * the first CPU is gathered by reading the first number in /sys/devices/system/node/node(uncore)/cpulist.
 * /dev/cpu/(cpu)/msr[-safe] must be writable
 */
static freq_gen_single_device_t  freq_gen_msr_device_init_uncore( int uncore )
{
	char buffer [BUFFER_SIZE];
	char * tail;
	/* get uncore id */


	if ( snprintf(buffer,BUFFER_SIZE,"/sys/devices/system/node/node%d/cpulist",uncore) == BUFFER_SIZE )
		return -ENOMEM;

	int fd = open(buffer, O_RDONLY);
	if ( fd < 0 )
	{
		return -EIO;
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
		return -EIO;
	}

	if ( snprintf(buffer,BUFFER_SIZE,"/dev/cpu/%ld/msr",cpu) == BUFFER_SIZE )
		return -ENOMEM;

	fd = open(buffer, O_WRONLY);
	if ( fd < 0 )
	{
		if ( snprintf(buffer,BUFFER_SIZE,"/dev/cpu/%ld/msr-safe",cpu) == BUFFER_SIZE )
			return -ENOMEM;
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

/* will write the frequency to the MSR */
static long long int freq_gen_msr_get_frequency(freq_gen_single_device_t fp)
{
	long long int setting = 0;
	int result=pread(fp,&setting,8,IA32_PERF_STATUS);

	if (result==8)
		return ((setting>>8) & 0xFF ) * 100000000;
	else
		return -EIO;
}

/* will write the frequency to the MSR */
static int freq_gen_msr_set_frequency(freq_gen_single_device_t fp, freq_gen_setting_t setting_in)
{
	long long int * setting = (long long int *) setting_in;
	int result=pwrite(fp,setting,8,IA32_PERF_CTL);

	if (result==8)
		return 0;
	else
		return -result;
}

/* will write the frequency to the MSR */
static long long int freq_gen_msr_get_frequency_uncore(freq_gen_single_device_t fp)
{
	long long int setting = 0;
	int result=pread(fp,&setting,8,UNCORE_RATIO_LIMIT);

	if (result==8)
		return (setting&0x77)*100000000;
	else
		return -EIO;
}


/* will allocate a small datastructure, containing freq information for uncore min and max */
static freq_gen_setting_t freq_gen_msr_prepare_access_uncore(long long target,int turbo)
{
	long long int * setting = malloc(sizeof(long long int));
	*setting=((target)/100000000)+(((target)/100000000)<<8);
	return setting;
}

/* will write the uncore frequency to min/max fields of the MSR */
static int freq_gen_msr_set_frequency_uncore(freq_gen_single_device_t fp, freq_gen_setting_t setting_in)
{
	int result=pwrite(fp,setting_in,8,UNCORE_RATIO_LIMIT);
	if ( result==8 )
		return 0;
	else
		return EIO;
}

/* frees datastructures that are prepared via freq_gen_msr_prepare_access(_uncore) */
static void freq_gen_msr_unprepare_access(freq_gen_setting_t setting)
{
	free(setting);
}

/* closes an open file descriptor */
static void freq_gen_msr_close_file(freq_gen_single_device_t fd, int cpu)
{
	close(fd);
}

/* no allocate variables :) nothing to do */
static void freq_gen_msr_finalize() {}

static freq_gen_interface_t freq_gen_msr_cpu_interface =
{
		.name = "msrsafe-entries",
		.init_device = freq_gen_msr_device_init,
		.get_num_devices = freq_gen_msr_get_max_entries,
		.prepare_set_frequency = freq_gen_msr_prepare_access,
		.get_frequency =freq_gen_msr_get_frequency,
		.set_frequency = freq_gen_msr_set_frequency,
		.unprepare_set_frequency = freq_gen_msr_unprepare_access,
		.close_device = freq_gen_msr_close_file,
		.finalize=freq_gen_msr_finalize
};

static freq_gen_interface_t freq_gen_msr_uncore_interface =
{
		.name = "msrsafe-entries",
		.init_device = freq_gen_msr_device_init_uncore,
		.get_num_devices = freq_gen_get_num_uncore,
		.prepare_set_frequency = freq_gen_msr_prepare_access_uncore,
		.get_frequency =freq_gen_msr_get_frequency_uncore,
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
