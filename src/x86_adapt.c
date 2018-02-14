/*
 * x86_adapt.c
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
#include <errno.h>

#include <x86_adapt.h>


#include "freq_gen_internal.h"

static freq_gen_interface_t freq_gen_x86a_cpu_interface;
static freq_gen_interface_t freq_gen_x86a_uncore_interface;

/* whether x86_adapt_init already called */
static int already_initialized;

/* index for set frequency */
static int xa_index_cpu;
/* index for get frequency */
static int xa_index_cpu_get;

/* index for uncore min */
static int xa_index_uncore_low;
/* index for uncore max */
static int xa_index_uncore_high;

/* whether one of these is initialized and used, only when both are not used any more x86a may be finalized */
static int core_is_initialized, uncore_is_initialized;

/* initialize change core frequency */
static int freq_gen_x86a_init_cpu( void )
{
	int ret = 0;

	/* initialize */
	if ( ! already_initialized )
		ret = x86_adapt_init();

	if ( ret == 0 )
	{
		/* search for core freq parameters */
		xa_index_cpu = x86_adapt_lookup_ci_name(X86_ADAPT_CPU, "Intel_Target_PState");
		if (xa_index_cpu < 0 )
		{
			if (!uncore_is_initialized)
				x86_adapt_finalize();
			return -xa_index_cpu;
		}

		xa_index_cpu_get = x86_adapt_lookup_ci_name(X86_ADAPT_CPU, "Intel_Current_PState");
		if (xa_index_cpu_get < 0 )
		{
			if (!uncore_is_initialized)
				x86_adapt_finalize();
			return -xa_index_cpu_get;
		}

		already_initialized = 1;
		core_is_initialized = 1;
		return 0;
	}
	return ret;

}
static int  freq_gen_x86a_init_uncore( void )
{
	int ret = 0;

	/* initialize */
	if ( ! already_initialized )
		ret = x86_adapt_init();

	if ( ret == 0 )
	{
		/* search for uncore freq parameters */
		xa_index_uncore_low = x86_adapt_lookup_ci_name(X86_ADAPT_DIE, "Intel_UNCORE_MIN_RATIO");
		if (xa_index_uncore_low < 0 )
		{
			if (!core_is_initialized)
				x86_adapt_finalize();
			return -xa_index_uncore_low;
		}
		xa_index_uncore_high = x86_adapt_lookup_ci_name(X86_ADAPT_DIE, "Intel_UNCORE_MAX_RATIO");
		if (xa_index_uncore_high < 0 )
		{
			if (!core_is_initialized)
				x86_adapt_finalize();
			return -xa_index_uncore_high;
		}
		already_initialized = 1;
		uncore_is_initialized = 1;
		return 0;
	}
	return ret;

}

static freq_gen_single_device_t freq_gen_x86a_init_cpu_device(int cpu)
{
	return x86_adapt_get_device(X86_ADAPT_CPU, cpu);
}

static freq_gen_single_device_t freq_gen_x86a_init_uncore_device(int cpu)
{
	return x86_adapt_get_device(X86_ADAPT_DIE, cpu);
}

static int freq_gen_x86a_get_max_cpus( freq_gen_interface_t param  )
{
	return x86_adapt_get_nr_avaible_devices(X86_ADAPT_CPU);
}

static int freq_gen_x86a_get_max_cpus_uncore( freq_gen_interface_t param  )
{
	return x86_adapt_get_nr_avaible_devices(X86_ADAPT_DIE);
}

static freq_gen_setting_t freq_gen_x86a_prepare_access(long long int target,int turbo)
{
	unsigned long long * setting= malloc(sizeof(unsigned long long));
	if ( setting == NULL)
	{
		return NULL;
	}
	*setting = target/100000000;
	return setting;
}


static long long int freq_gen_x86_get_frequency(freq_gen_single_device_t fp)
{
	uint64_t frequency;
	int result=x86_adapt_get_setting((int)fp,xa_index_cpu_get,&frequency);
	if (result==8)
		return ( frequency >> 8 ) * 100000000;
	else
		return -EIO;
}

static int freq_gen_x86_set_frequency(freq_gen_single_device_t fp, freq_gen_setting_t setting_in)
{
	unsigned long long * target= (unsigned long long* ) setting_in;
	int result=x86_adapt_set_setting((int)fp,xa_index_cpu,*target);
	if (result==8)
		return 0;
	else
		return -result;
}

static long long int freq_gen_x86_get_frequency_uncore(freq_gen_single_device_t fp)
{
	uint64_t frequency;
	int result=x86_adapt_get_setting((int)fp,xa_index_uncore_high,&frequency);
	if (result==8)
		return frequency * 100000000;
	else
		return -EIO;
}

static int freq_gen_x86_set_frequency_uncore(freq_gen_single_device_t fp, freq_gen_setting_t setting_in)
{
	unsigned long long * target= (unsigned long long* ) setting_in;
	int result=x86_adapt_set_setting((int)fp,xa_index_uncore_low,*target);
	int result2=x86_adapt_set_setting((int)fp,xa_index_uncore_high,*target);
	if ( result==8 && result2 == 8)
		return 0;
	else
		return EIO;
}

static void freq_gen_x86a_unprepare_access(freq_gen_setting_t setting_in)
{
	free(setting_in);
}

static void freq_gen_x86a_close_file(int cpu_nr,freq_gen_single_device_t fp)
{
	x86_adapt_put_device(X86_ADAPT_CPU,cpu_nr);
}
static void freq_gen_x86a_close_file_uncore(int uncore_nr,freq_gen_single_device_t fp)
{
	x86_adapt_put_device(X86_ADAPT_DIE,uncore_nr);
}


static void freq_gen_x86a_finalize_core()
{
	core_is_initialized = 0;
	if ( !uncore_is_initialized )
	{
		x86_adapt_finalize();
	}
}

static void freq_gen_x86a_finalize_uncore()
{
	uncore_is_initialized = 0;
	if ( !core_is_initialized )
	{
		x86_adapt_finalize();
	}
}

static freq_gen_interface_t freq_gen_x86a_cpu_interface =
{
		.name = "x86_adapt",
		.init_device = freq_gen_x86a_init_cpu_device,
		.get_num_devices = freq_gen_x86a_get_max_cpus,
		.prepare_set_frequency = freq_gen_x86a_prepare_access,
		.get_frequency = freq_gen_x86_get_frequency,
		.set_frequency = freq_gen_x86_set_frequency,
		.unprepare_set_frequency = freq_gen_x86a_unprepare_access,
		.close_device = freq_gen_x86a_close_file,
		.finalize = freq_gen_x86a_finalize_core
};

static freq_gen_interface_t * freq_gen_x86a_init_cpufreq( void )
{
	int ret=freq_gen_x86a_init_cpu();
	if ( ret )
	{
		errno=ret;
		return NULL;
	}
	return &freq_gen_x86a_cpu_interface;
}

static freq_gen_interface_t freq_gen_x86a_uncore_interface =
{
		.name = "x86_adapt",
		.init_device = freq_gen_x86a_init_uncore_device,
		.get_num_devices = freq_gen_x86a_get_max_cpus_uncore,
		.prepare_set_frequency = freq_gen_x86a_prepare_access,
		.get_frequency = freq_gen_x86_get_frequency_uncore,
		.set_frequency = freq_gen_x86_set_frequency_uncore,
		.unprepare_set_frequency = freq_gen_x86a_unprepare_access,
		.close_device = freq_gen_x86a_close_file_uncore,
		.finalize = freq_gen_x86a_finalize_uncore
};

static freq_gen_interface_t * freq_gen_x86a_init_uncorefreq( void )
{
	int ret=freq_gen_x86a_init_uncore();
	if ( ret )
	{
		errno=ret;
		return NULL;
	}
	return &freq_gen_x86a_uncore_interface;
}

freq_gen_interface_internal_t freq_gen_x86a_interface_internal =
{
		.init_cpufreq = freq_gen_x86a_init_cpufreq,
		.init_uncorefreq = freq_gen_x86a_init_uncorefreq

};

