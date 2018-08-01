/*
 * freq_gen.c
 *
 * This file implements the interface between the internal libfreqgen-generators (sysfs,msr,...)
 * and the user that accesses the library
 *
 *  Created on: 26.01.2018
 *      Author: rschoene
 */

#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "freq_gen_internal.h"

/* store previously set core and uncore to be able to iterate through them */
static int previous_core = -1;
static int previous_uncore = -1;

static bool is_selected_core_interface(char * name)
{
    static bool selected_core_interface = false;
    static char * core_interface = NULL;
    if (! selected_core_interface)
    {
        core_interface = getenv("LIBFREQGEN_CORE_INTERFACE");
        selected_core_interface = true;
    }

    if (core_interface == NULL)
        return true;
    if (strcmp(name,core_interface) == 0)
        return true;
    else
        return false;
}

static bool is_selected_uncore_interface(char * name)
{
    static bool selected_uncore_interface = false;
    static char * uncore_interface = NULL;
    if (! selected_uncore_interface)
    {
        uncore_interface = getenv("LIBFREQGEN_UNCORE_INTERFACE");
        selected_uncore_interface = true;
    }

    if (uncore_interface == NULL)
        return true;
    if (strcmp(name,uncore_interface) == 0)
        return true;
    else
        return false;
}

freq_gen_interface_t* freq_gen_init(freq_gen_dev_type type)
{
    /* this needs to be increased whenever there's a new implementation */
    int nr_avail = 2
#ifdef USEX86_ADAPT
                   + 1
#endif
#ifdef USELIKWID
                   + 1
#endif
        ;
    /* new implementations will be appended here and added to freq_gen_internal.h */
    freq_gen_interface_internal_t* avail[] = { &freq_gen_sysfs_interface_internal,
                                               &freq_gen_msr_interface_internal
#ifdef USEX86_ADAPT
                                               ,
                                               &freq_gen_x86a_interface_internal
#endif
#ifdef USELIKWID
                                               ,
                                               &freq_gen_likwid_interface_internal
#endif

    };

    switch (type)
    {
    /* go through */
    case FREQ_GEN_DEVICE_CORE_FREQ:
        for (int i = previous_core + 1; i < nr_avail; i++)
        {
            if (avail[i]->init_cpufreq != NULL && is_selected_core_interface(avail[i]->name))
            {
                freq_gen_interface_t* found = avail[i]->init_cpufreq();
                if (found)
                {
                    previous_core = i;
                    return found;
                }
            }
        }
        /* so that we can start again next time */
        if (previous_core >= nr_avail)
            previous_core = -1;
        return NULL;

    case FREQ_GEN_DEVICE_UNCORE_FREQ:
        for (int i = previous_uncore + 1; i < nr_avail; i++)
        {
            if (avail[i]->init_uncorefreq != NULL && is_selected_uncore_interface(avail[i]->name))
            {
                freq_gen_interface_t* found = avail[i]->init_uncorefreq();
                if (found)
                {
                    previous_uncore = i;
                    return found;
                }
            }
        }
        /* so that we can start again next time */
        if (previous_uncore >= nr_avail)
            previous_uncore = -1;
        return NULL;

    default:
        return NULL;
    }
}
