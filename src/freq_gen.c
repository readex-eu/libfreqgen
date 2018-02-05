/*
 * freq_gen.c
 *
 *  Created on: 26.01.2018
 *      Author: rschoene
 */

#include <stddef.h>

#include "freq_gen_internal.h"




freq_gen_interface_t * freq_gen_init(freq_gen_dev_type type)
{
	static int previous_core = -1;
	static int previous_uncore = -1;
	int  nr_avail = 4;
	freq_gen_interface_internal_t * avail[] =
	{
			&freq_gen_sysfs_interface_internal,
			&freq_gen_x86a_interface_internal,
			&freq_gen_msr_interface_internal,
			&freq_gen_likwid_interface_internal
	};
	if ( type == FREQ_GEN_DEVICE_CORE_FREQ )
		for (int i=previous_core+1; i< nr_avail; i++)
		{
			if ( avail[i]->init_cpufreq != NULL )
			{
				freq_gen_interface_t * found = avail[i]->init_cpufreq();
				if ( found )
				{
					previous_core=i;
					return found;
				}
			}
		}
	if ( type == FREQ_GEN_DEVICE_UNCORE_FREQ )
		for (int i=previous_uncore+1; i< nr_avail; i++)
		{
			if ( avail[i]->init_uncorefreq != NULL )
			{
				freq_gen_interface_t * found = avail[i]->init_uncorefreq();
				if ( found )
				{
					previous_uncore=i;
					return found;
				}
			}
		}
	return NULL;

}
