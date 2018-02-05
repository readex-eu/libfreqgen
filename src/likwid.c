/*
 * likwid.c
 *
 *  Created on: 26.01.2018
 *      Author: rschoene
 */



#include <stddef.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

#include <likwid.h>


#include "freq_gen_internal.h"


#define STEPPING(eax) (eax & 0xF)
#define MODEL(eax) ((eax >> 4) & 0xF)
#define FAMILY(eax) ((eax >> 8) & 0xF)
#define TYPE(eax) ((eax >> 12) & 0x3)
#define EXT_MODEL(eax) ((eax >> 16) & 0xF)
#define EXT_FAMILY(eax) ((eax >> 20) & 0xFF)

#define IA32_PERF_CTL 0x199
#define UNCORE_RATIO_LIMIT 0x620

static freq_gen_interface_t freq_gen_likwid_cpu_interface;
static freq_gen_interface_t freq_gen_likwid_uncore_interface;


static inline void cpuid(unsigned int *eax, unsigned int *ebx,
                         unsigned int *ecx, unsigned int *edx)
{
        /* ecx is often an input as well as an output. */
        asm volatile("cpuid"
            : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
            : "0" (*eax), "2" (*ecx));
}

static int freq_gen_likwid_get_max_entries(   )
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
			if (access (buffer, F_OK) != 0)
			{

				/* check access to msr */
				if (snprintf(buffer,BUFFER_SIZE,"/dev/cpu/%lli/msr-safe",current) == BUFFER_SIZE)
				{
					closedir(dir);
					return -ENOMEM;
				}
				if (access (buffer, F_OK) != 0)
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

static int initialized;

static freq_gen_interface_t * freq_gen_likwid_init( void )
{
	HPMmode(ACCESSMODE_DAEMON);
	if (!initialized)
	{
		int ret = HPMinit();
		if (ret == 0)
		{
			initialized = 1;
			return &freq_gen_likwid_cpu_interface;
		}
		else
		{
			errno=ret;
			return NULL;
		}
	}
	else
	{
		return &freq_gen_likwid_cpu_interface;
	}

}

static freq_gen_interface_t * freq_gen_likwid_init_uncore( void )
{
	HPMmode(ACCESSMODE_DAEMON);
	if (!initialized)
	{
		int ret = HPMinit();
		if (ret == 0)
		{
			initialized = 1;
			return &freq_gen_likwid_uncore_interface;
		}
		else
		{
			errno=ret;
			return NULL;
		}
	}
	else
	{
		return &freq_gen_likwid_uncore_interface;
	}
}

static char* avail_freqs;

static freq_gen_single_device_t freq_gen_likwid_device_init( int cpu_id )
{
	int ret = HPMaddThread(cpu_id);
	if ( ret == 0 )
	{
		if (avail_freqs == NULL)
			avail_freqs = freq_getAvailFreq(cpu_id);
		printf("%s\n",avail_freqs);
		return cpu_id;
	}
	else
	{
		return ret;
	}

}

static freq_gen_single_device_t  freq_gen_likwid_device_init_uncore( int uncore )
{
	return uncore;
}

static freq_gen_setting_t freq_gen_likwid_prepare_access(long long target,int turbo)
{
	uint64_t current_u=0;
	target=target/1000;
	char* token = strtok(avail_freqs," ");
	char * end;
	while (token != NULL)
	{
		double current = strtod(token,&end)*1000.0;
		current_u = (uint64_t) current;
		current_u=current_u*1000;
		if (current_u > target)
			break;
		token = strtok(NULL," ");
	}
	if (current_u < target )
	{
		return NULL;
	}
	uint64_t * setting = malloc(sizeof(double));
	*setting=(current_u);
	return setting;
}


static freq_gen_setting_t freq_gen_likwid_prepare_access_uncore(long long target,int turbo)
{
	uint64_t * setting = malloc(sizeof(uint64_t));
	*setting=(target/1000000);
	return setting;
}

static int freq_gen_likwid_set_frequency(freq_gen_single_device_t fp, freq_gen_setting_t setting_in)
{
	uint64_t * setting = (uint64_t *) setting_in;
#ifdef AVOID_LIKWID_BUG
	freq_setCpuClockMin(fp, *setting);
	freq_setCpuClockMax(fp, *setting);
#else /* AVOID_LIKWID_BUG */
	uint64_t set_freq = freq_setCpuClockMin(fp, *setting);
	if ( set_freq == 0 )
	{
		return EIO;
	}
	set_freq = freq_setCpuClockMax(fp, *setting);
	if ( set_freq == 0 )
	{
		return EIO;
	}
#endif /* AVOID_LIKWID_BUG */
	return 0;
}

static int freq_gen_likwid_set_frequency_uncore(freq_gen_single_device_t fp, freq_gen_setting_t setting_in)
{
	uint64_t * setting = (uint64_t *) setting_in;
#ifdef AVOID_LIKWID_BUG
	freq_setUncoreFreqMin(fp, *setting);
	freq_setUncoreFreqMax(fp, *setting);
#else /* AVOID_LIKWID_BUG */
	uint64_t set_freq = freq_setUncoreFreqMin(fp, *setting);
	if ( set_freq == 0 )
	{
		return EIO;
	}
	set_freq = freq_setUncoreFreqMax(fp, *setting);
	if ( set_freq == 0 )
	{
		return EIO;
	}
#endif /* AVOID_LIKWID_BUG */
	return 0;
}

static void freq_gen_likwid_unprepare_access(freq_gen_setting_t setting)
{
	if (avail_freqs)
		free(avail_freqs);

	free(setting);
}

static void freq_gen_likwid_close_file(freq_gen_single_device_t fd, int cpu) {}

static void freq_gen_likwid_finalize()
{
	HPMfinalize();
}

static freq_gen_interface_t freq_gen_likwid_cpu_interface =
{
		.name = "likwid-entries",
		.init_device = freq_gen_likwid_device_init,
		.get_num_devices = freq_gen_likwid_get_max_entries,
		.prepare_set_frequency = freq_gen_likwid_prepare_access,
		.set_frequency = freq_gen_likwid_set_frequency,
		.unprepare_set_frequency = freq_gen_likwid_unprepare_access,
		.close_device = freq_gen_likwid_close_file,
		.finalize=freq_gen_likwid_finalize
};

static freq_gen_interface_t freq_gen_likwid_uncore_interface =
{
		.name = "likwid-entries",
		.init_device = freq_gen_likwid_device_init_uncore,
		.get_num_devices = freq_gen_likwid_get_max_entries,
		.prepare_set_frequency = freq_gen_likwid_prepare_access_uncore,
		.set_frequency = freq_gen_likwid_set_frequency_uncore,
		.unprepare_set_frequency = freq_gen_likwid_unprepare_access,
		.close_device = freq_gen_likwid_close_file,
		.finalize=freq_gen_likwid_finalize
};

freq_gen_interface_internal_t freq_gen_likwid_interface_internal =
{
		.init_cpufreq = freq_gen_likwid_init,
		.init_uncorefreq = freq_gen_likwid_init_uncore
};
