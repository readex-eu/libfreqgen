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

static int already_initialized=0;

static int xa_index_cpu=0;
static int xa_index_uncore_low=0;
static int xa_index_uncore_high=0;


static int freq_gen_x86a_init_cpu( void )
{
	int ret = 0;

	if ( ! already_initialized )
		ret = x86_adapt_init();

	if ( ret == 0 )
	{
		already_initialized = 1;
		xa_index_cpu = x86_adapt_lookup_ci_name(X86_ADAPT_CPU, "Intel_Target_PState");
		if (xa_index_cpu < 0 )
		{
			return -xa_index_cpu;
		}
		return 0;
	}
	return ret;

}
static int  freq_gen_x86a_init_uncore( void )
{
	int ret = 0;

	if ( ! already_initialized )
		ret = x86_adapt_init();

	if ( ret == 0 )
	{
		already_initialized = 1;
		xa_index_uncore_low = x86_adapt_lookup_ci_name(X86_ADAPT_DIE, "Intel_UNCORE_MIN_RATIO");
		if (xa_index_uncore_low < 0 )
		{
			return -xa_index_uncore_low;
		}
		xa_index_uncore_high = x86_adapt_lookup_ci_name(X86_ADAPT_DIE, "Intel_UNCORE_MAX_RATIO");
		if (xa_index_uncore_high < 0 )
		{
			return -xa_index_uncore_high;
		}
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

struct setting
{
	unsigned long long setting;
	int id;
};

static freq_gen_setting_t freq_gen_x86a_prepare_access(long long int target,int turbo)
{
	struct setting * setting= malloc(sizeof(struct setting));
	if ( setting == NULL)
	{
		return NULL;
	}
	setting->setting = target/100000000;
	setting->id = xa_index_cpu;
	return setting;
}

static int freq_gen_x86_set_frequency(freq_gen_single_device_t fp, freq_gen_setting_t setting_in)
{
	struct setting * target= (struct setting* ) setting_in;
	int result=x86_adapt_set_setting((int)fp,target->id,target->setting);
	if (result==8)
		return 0;
	else
		return -result;
}

struct setting_uncore
{
	unsigned long long setting;
	int low;
	int high;
};


static freq_gen_setting_t freq_gen_x86a_prepare_access_uncore(long long int target,int turbo)
{
	struct setting_uncore * setting= malloc(sizeof(struct setting_uncore));
	if ( setting == NULL)
	{
		return NULL;
	}
	setting->setting = target/100000000;
	setting->low = xa_index_uncore_low;
	setting->high = xa_index_uncore_low;
	return setting;
}

static int freq_gen_x86_set_frequency_uncore(freq_gen_single_device_t fp, freq_gen_setting_t setting_in)
{
	struct setting_uncore * target= (struct setting_uncore* ) setting_in;
	int result=x86_adapt_set_setting((int)fp,target->low,target->setting);
	int result2=x86_adapt_set_setting((int)fp,target->high,target->setting);
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

static freq_gen_interface_t freq_gen_x86a_cpu_interface =
{
		.name = "x86_adapt",
		.init_device = freq_gen_x86a_init_cpu_device,
		.get_num_devices = freq_gen_x86a_get_max_cpus,
		.prepare_set_frequency = freq_gen_x86a_prepare_access,
		.set_frequency = freq_gen_x86_set_frequency,
		.unprepare_set_frequency = freq_gen_x86a_unprepare_access,
		.close_device = freq_gen_x86a_close_file
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
		.prepare_set_frequency = freq_gen_x86a_prepare_access_uncore,
		.set_frequency = freq_gen_x86_set_frequency_uncore,
		.unprepare_set_frequency = freq_gen_x86a_unprepare_access,
		.close_device = freq_gen_x86a_close_file_uncore
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

