/*
 * likwid.c
 *
 *Likwid implementation for setting core and uncore frequency via the ACCESSMODE_DAEMON
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
#include "freq_gen_internal_generic.h"

/* implementations of the interface */
static freq_gen_interface_t freq_gen_likwid_cpu_interface;
static freq_gen_interface_t freq_gen_likwid_uncore_interface;

/* whether this is initialized */
static int initialized;

/* this will store the avail freqs as char array */
static char* avail_freqs;

/* this will return the maximal number of CPUs by looking for /dev/cpu/(nr)/msr[-safe]
 * time complexity is O(num_cpus) for the first call. Afterwards its O(1), since the return value is buffered
 */
static int freq_gen_likwid_get_max_entries(   )
{
	static long long int max = -1;
	if ( max != -1 )
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
	max=max+1;
	return max;
}

/* will initialize the core frequency stuff
 * If it is unable to connect (HPMinit returns != 0), it will set errno and return NULL
 */
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


/* will initialize the core frequency stuff
 * If it is unable to connect (HPMinit returns != 0), it will set errno and return NULL
 */
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

/* this will add a thread to access the msr and read the available frequencies for the given cpu_id
 * Getting the available frequencies will only be done for the first cpu_id that is passed. The returned
 * value will be used for all other CPUs
 * */
static freq_gen_single_device_t freq_gen_likwid_device_init( int cpu_id )
{
	int ret = HPMaddThread(cpu_id);
	if ( ret == 0 )
	{
		if (avail_freqs == NULL)
			avail_freqs = freq_getAvailFreq(cpu_id);
		return cpu_id;
	}
	else
	{
		return ret;
	}

}
/* will just return the uncore */
static freq_gen_single_device_t  freq_gen_likwid_device_init_uncore( int uncore )
{
	return uncore;
}

/* prepares the setting for core frequencies by checking which is the first frequency available
 * that's equal or higher the proposed frequency
 * O(strlen(avail_frequencies))+malloc
 * turbo is ignored
 */
static freq_gen_setting_t freq_gen_likwid_prepare_access( long long target , int turbo )
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


/* prepares the setting for uncore frequencies
 * O(malloc)
 * turbo is ignored
 */
static freq_gen_setting_t freq_gen_likwid_prepare_access_uncore(long long target,int turbo)
{
	uint64_t * setting = malloc(sizeof(uint64_t));
	*setting=(target/1000000);
	return setting;
}

static long long int freq_gen_likwid_get_frequency(freq_gen_single_device_t fp)
{
	int frequency = freq_getCpuClockMax( fp );
	if ( frequency == 0 )
	{
		return -EIO;
	}
	else
	{
		return frequency;
	}
}

static long long int freq_gen_likwid_get_min_frequency(freq_gen_single_device_t fp)
{
    int frequency = freq_getCpuClockMin( fp );
    if ( frequency == 0 )
    {
        return -EIO;
    }
    else
    {
        return frequency;
    }
}

/* applies core frequency setting
 * O(freq_setCpuClockMin)+O(freq_setCpuClockMax)
 * If AVOID_LIKWID_BUG is activated during compilation, return codes are not checked
 */
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

/* applies core frequency setting
 * O(freq_setCpuClockMin)+O(freq_setCpuClockMax)
 * If AVOID_LIKWID_BUG is activated during compilation, return codes are not checked
 */
static int freq_gen_likwid_set_min_frequency(freq_gen_single_device_t fp, freq_gen_setting_t setting_in)
{
    uint64_t * setting = (uint64_t *) setting_in;
#ifdef AVOID_LIKWID_BUG
    freq_setCpuClockMin(fp, *setting);
#else /* AVOID_LIKWID_BUG */
    uint64_t set_freq = freq_setCpuClockMin(fp, *setting);
    if ( set_freq == 0 )
    {
        return EIO;
    }
#endif /* AVOID_LIKWID_BUG */
    return 0;
}

static long long int freq_gen_likwid_get_frequency_uncore(freq_gen_single_device_t fp)
{
	uint64_t frequency = freq_getUncoreFreqMax( fp );
	if ( frequency == 0 )
	{
		return -EIO;
	}
	else
	{
		return frequency * 1000000;
	}
}

static long long int freq_gen_likwid_get_min_frequency_uncore(freq_gen_single_device_t fp)
{
    uint64_t frequency = freq_getUncoreFreqMin( fp );
    if ( frequency == 0 )
    {
        return -EIO;
    }
    else
    {
        return frequency * 1000000;
    }
}

/* applies uncore frequency setting
 * O(freq_setUncoreFreqMin)+O(freq_setUncoreFreqMax)
 * If AVOID_LIKWID_BUG is activated during compilation, return codes are not checked
 */
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

static int freq_gen_likwid_set_min_frequency_uncore(freq_gen_single_device_t fp, freq_gen_setting_t setting_in)
{
    uint64_t * setting = (uint64_t *) setting_in;
#ifdef AVOID_LIKWID_BUG
    freq_setUncoreFreqMin(fp, *setting);
#else /* AVOID_LIKWID_BUG */
    uint64_t set_freq = freq_setUncoreFreqMin(fp, *setting);
    if ( set_freq == 0 )
    {
        return EIO;
    }
#endif /* AVOID_LIKWID_BUG */
    return 0;
}

/* Just free some data structure */
static void freq_gen_likwid_unprepare_access(freq_gen_setting_t setting)
{
	free(setting);
}

/* The daemon will do it, so nothing to do here */
static void freq_gen_likwid_do_nothing(freq_gen_single_device_t fd, int cpu) {}

/* close connection to access daemon and free some data structures */
static void freq_gen_likwid_finalize()
{
	HPMfinalize();
	if (avail_freqs)
		free(avail_freqs);
}

static freq_gen_interface_t freq_gen_likwid_cpu_interface =
{
		.name = "likwid-entries",
		.init_device = freq_gen_likwid_device_init,
		.get_num_devices = freq_gen_likwid_get_max_entries,
		.prepare_set_frequency = freq_gen_likwid_prepare_access,
		.get_frequency = freq_gen_likwid_get_frequency,
        .get_min_frequency = freq_gen_likwid_get_min_frequency,
        .set_frequency = freq_gen_likwid_set_frequency,
        .set_min_frequency = freq_gen_likwid_set_min_frequency,
		.unprepare_set_frequency = freq_gen_likwid_unprepare_access,
		.close_device = freq_gen_likwid_do_nothing,
		.finalize=freq_gen_likwid_finalize
};

static freq_gen_interface_t freq_gen_likwid_uncore_interface =
{
		.name = "likwid-entries",
		.init_device = freq_gen_likwid_device_init_uncore,
		.get_num_devices = freq_gen_get_num_uncore,
		.prepare_set_frequency = freq_gen_likwid_prepare_access_uncore,
		.get_frequency = freq_gen_likwid_get_frequency_uncore,
        .get_min_frequency = freq_gen_likwid_get_min_frequency_uncore,
		.set_frequency = freq_gen_likwid_set_frequency_uncore,
        .set_min_frequency = freq_gen_likwid_set_min_frequency_uncore,
		.unprepare_set_frequency = freq_gen_likwid_unprepare_access,
		.close_device = freq_gen_likwid_do_nothing,
		.finalize=freq_gen_likwid_do_nothing
};


freq_gen_interface_internal_t freq_gen_likwid_interface_internal =
{
		.init_cpufreq = freq_gen_likwid_init,
		.init_uncorefreq = freq_gen_likwid_init_uncore
};
