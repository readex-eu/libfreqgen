/*
 * x86_adapt.c
 *
 *  Created on: 26.01.2018
 *      Author: rschoene
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <x86_adapt.h>

#include "freq_gen_internal.h"
#include "../include/error.h"

static freq_gen_interface_t freq_gen_x86a_cpu_interface;
static freq_gen_interface_t freq_gen_x86a_uncore_interface;

/* whether x86_adapt_init already called */
static int already_initialized;

/* index for set frequency */
static int xa_index_cpu;

/* index for uncore min */
static int xa_index_uncore_low;
/* index for uncore max */
static int xa_index_uncore_high;

/* whether one of these is initialized and used, only when both are not used any more x86a may be
 * finalized */
static int core_is_initialized, uncore_is_initialized;

static int is_newer = 1;

/* some definitions to parse cpuid */
#define STEPPING(eax) (eax & 0xF)
#define MODEL(eax) ((eax >> 4) & 0xF)
#define FAMILY(eax) ((eax >> 8) & 0xF)
#define TYPE(eax) ((eax >> 12) & 0x3)
#define EXT_MODEL(eax) ((eax >> 16) & 0xF)
#define EXT_FAMILY(eax) ((eax >> 20) & 0xFF)

/* cpuid call in C */
static inline void cpuid(unsigned int* eax, unsigned int* ebx, unsigned int* ecx, unsigned int* edx)
{
    /* ecx is often an input as well as an output. */
    __asm__ volatile("cpuid" : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx) : "0"(*eax), "2"(*ecx));
}

/* check whether frequency scaling for the current CPU is supported
 * returns 0 if not or 1 if so
 * TODO: write error messages for this once return values have been implemented for this function
 */
static void check_processor()
{
    char buffer[13];
    unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
    cpuid(&eax, &ebx, &ecx, &edx);

    /* reorder name string */
    buffer[0] = ebx & 0xFF;
    buffer[1] = (ebx >> 8) & 0xFF;
    buffer[2] = (ebx >> 16) & 0xFF;
    buffer[3] = (ebx >> 24) & 0xFF;
    buffer[4] = edx & 0xFF;
    buffer[5] = (edx >> 8) & 0xFF;
    buffer[6] = (edx >> 16) & 0xFF;
    buffer[7] = (edx >> 24) & 0xFF;
    buffer[8] = ecx & 0xFF;
    buffer[9] = (ecx >> 8) & 0xFF;
    buffer[10] = (ecx >> 16) & 0xFF;
    buffer[11] = (ecx >> 24) & 0xFF;
    buffer[12] = '\0';

    if (strcmp(buffer, "GenuineIntel") == 0)
    {
        eax = 1;
        cpuid(&eax, &ebx, &ecx, &edx);
        if (FAMILY(eax) != 6)
            return;
        switch ((EXT_MODEL(eax) << 4) + MODEL(eax))
        {
        /* Sandy Bridge */
        case 0x2a:
        case 0x2d:
        /* Ivy Bridge */
        case 0x3a:
        case 0x3e:
            is_newer = 0;
            break;
        default:
            break;
        }
    }
}

/* initialize change core frequency */
static int freq_gen_x86a_init_cpu(void)
{
    int ret = 0;

    check_processor();
    /* initialize */
    if (!already_initialized)
        ret = x86_adapt_init();

    if (ret == 0)
    {
        /* search for core freq parameters */
        xa_index_cpu = x86_adapt_lookup_ci_name(X86_ADAPT_CPU, "Intel_Target_PState");
        if (xa_index_cpu < 0)
        {
            if (!uncore_is_initialized)
                x86_adapt_finalize();
            LIBFREQGEN_SET_ERROR("invalid cpu returned by x86_adapt_lookup_ci_name for Intel_Target_PState");
            return -xa_index_cpu;
        }

        already_initialized = 1;
        core_is_initialized = 1;
        return 0;
    } else
    {
    	LIBFREQGEN_SET_ERROR("could not initialize x86_adapt");
    }
    return ret;
}
static int freq_gen_x86a_init_uncore(void)
{
    int ret = 0;

    check_processor();
    /* initialize */
    if (!already_initialized)
        ret = x86_adapt_init();

    if (ret == 0)
    {
        /* search for uncore freq parameters */
        xa_index_uncore_low = x86_adapt_lookup_ci_name(X86_ADAPT_DIE, "Intel_UNCORE_MIN_RATIO");
        if (xa_index_uncore_low < 0)
        {
            if (!core_is_initialized)
                x86_adapt_finalize();
            LIBFREQGEN_SET_ERROR("invalid index returned by x86_adapt_lookup_ci_name for Intel_UNCORE_MIN_RATIO");
            return -xa_index_uncore_low;
        }
        xa_index_uncore_high = x86_adapt_lookup_ci_name(X86_ADAPT_DIE, "Intel_UNCORE_MAX_RATIO");
        if (xa_index_uncore_high < 0)
        {
            if (!core_is_initialized)
                x86_adapt_finalize();
            LIBFREQGEN_SET_ERROR("invalid index returned by x86_adapt_lookup_ci_name for Intel_UNCORE_MAX_RATIO");
            return -xa_index_uncore_high;
        }
        already_initialized = 1;
        uncore_is_initialized = 1;
        return 0;
    } else
    {
    	LIBFREQGEN_SET_ERROR("could not initialize x86_adapt");
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

static int freq_gen_x86a_get_max_cpus(freq_gen_interface_t param)
{
    return x86_adapt_get_nr_avaible_devices(X86_ADAPT_CPU);
}

static int freq_gen_x86a_get_max_cpus_uncore(freq_gen_interface_t param)
{
    return x86_adapt_get_nr_avaible_devices(X86_ADAPT_DIE);
}

static freq_gen_setting_t freq_gen_x86a_prepare_access(long long int target, int turbo)
{
    unsigned long long* setting = malloc(sizeof(unsigned long long));
    if (setting == NULL)
    {
    	LIBFREQGEN_SET_ERROR("could not allocate %d bytes of memory", sizeof(unsigned long long));
        return NULL;
    }
    if (is_newer)
        *setting = (target / 100000000) << 8;
    else
        *setting = (target / 100000000);
    return setting;
}

static freq_gen_setting_t freq_gen_x86a_prepare_access_uncore(long long int target, int turbo)
{
    unsigned long long* setting = malloc(sizeof(unsigned long long));
    if (setting == NULL)
    {
    	LIBFREQGEN_SET_ERROR("could not allocate %d bytes of memory", sizeof(unsigned long long));
        return NULL;
    }
    *setting = (target / 100000000);
    return setting;
}

static long long int freq_gen_x86_get_frequency(freq_gen_single_device_t fp)
{
    uint64_t frequency;
    int result = x86_adapt_get_setting((int)fp, xa_index_cpu, &frequency);
    if (result == 8)
        if (is_newer)
            return (frequency >> 8) * 100000000;
        else
            return frequency * 100000000;
    else
    {
    	LIBFREQGEN_SET_ERROR("could not get cpu frequency from x86_adapt");
        return -EIO;
    }
}

static int freq_gen_x86_set_frequency(freq_gen_single_device_t fp, freq_gen_setting_t setting_in)
{
    unsigned long long* target = (unsigned long long*)setting_in;
    int result = x86_adapt_set_setting((int)fp, xa_index_cpu, *target);
    if (result == 8)
        return 0;
    else
    {
    	LIBFREQGEN_SET_ERROR("could not set value %llu to cpu", *target);
        return -result;
    }
}

static long long int freq_gen_x86_get_frequency_uncore(freq_gen_single_device_t fp)
{
    uint64_t frequency;
    int result = x86_adapt_get_setting((int)fp, xa_index_uncore_high, &frequency);
    if (result == 8)
        return frequency * 100000000;
    else
    {
    	LIBFREQGEN_SET_ERROR("could not get uncore frequency from x86_adapt");
        return -EIO;
    }
}

static long long int freq_gen_x86_get_min_frequency_uncore(freq_gen_single_device_t fp)
{
    uint64_t frequency;
    int result = x86_adapt_get_setting((int)fp, xa_index_uncore_low, &frequency);
    if (result == 8)
        return frequency * 100000000;
    else
    {
    	LIBFREQGEN_SET_ERROR("could not get uncore frequency from x86_adapt");
        return -EIO;
    }
}

static int freq_gen_x86_set_frequency_uncore(freq_gen_single_device_t fp,
                                             freq_gen_setting_t setting_in)
{
    unsigned long long* target = (unsigned long long*)setting_in;
    int result = x86_adapt_set_setting((int)fp, xa_index_uncore_low, *target);
    int result2 = x86_adapt_set_setting((int)fp, xa_index_uncore_high, *target);
    if (result == 8 && result2 == 8)
        return 0;
    else
    {
    	LIBFREQGEN_SET_ERROR("could not set uncore value %llu", *target);
        return EIO;
    }
}

static int freq_gen_x86_set_min_frequency_uncore(freq_gen_single_device_t fp,
                                                 freq_gen_setting_t setting_in)
{
    unsigned long long* target = (unsigned long long*)setting_in;
    int result = x86_adapt_set_setting((int)fp, xa_index_uncore_low, *target);
    if (result == 8)
        return 0;
    else
    {
    	LIBFREQGEN_SET_ERROR("could not set uncore value %llu", *target);
        return EIO;
    }
}

static void freq_gen_x86a_unprepare_access(freq_gen_setting_t setting_in)
{
    free(setting_in);
}

static void freq_gen_x86a_unprepare_access_uncore(freq_gen_setting_t setting_in)
{
    free(setting_in);
}

static void freq_gen_x86a_close_file(int cpu_nr, freq_gen_single_device_t fp)
{
    x86_adapt_put_device(X86_ADAPT_CPU, cpu_nr);
}
static void freq_gen_x86a_close_file_uncore(int uncore_nr, freq_gen_single_device_t fp)
{
    x86_adapt_put_device(X86_ADAPT_DIE, uncore_nr);
}

static void freq_gen_x86a_finalize_core()
{
    core_is_initialized = 0;
    if (!uncore_is_initialized)
    {
        x86_adapt_finalize();
    }
}

static void freq_gen_x86a_finalize_uncore()
{
    uncore_is_initialized = 0;
    if (!core_is_initialized)
    {
        x86_adapt_finalize();
    }
}

static freq_gen_interface_t freq_gen_x86a_cpu_interface = {
    .name = "x86_adapt",
    .init_device = freq_gen_x86a_init_cpu_device,
    .get_num_devices = freq_gen_x86a_get_max_cpus,
    .prepare_set_frequency = freq_gen_x86a_prepare_access,
    .get_frequency = freq_gen_x86_get_frequency,
    .get_min_frequency = NULL,
    .set_frequency = freq_gen_x86_set_frequency,
    .set_min_frequency = NULL,
    .unprepare_set_frequency = freq_gen_x86a_unprepare_access,
    .close_device = freq_gen_x86a_close_file,
    .finalize = freq_gen_x86a_finalize_core
};

static freq_gen_interface_t* freq_gen_x86a_init_cpufreq(void)
{
    int ret = freq_gen_x86a_init_cpu();
    if (ret)
    {
        errno = ret;
        LIBFREQGEN_SET_ERROR("could not initialize x86a/cpu");
        return NULL;
    }
    return &freq_gen_x86a_cpu_interface;
}

static freq_gen_interface_t freq_gen_x86a_uncore_interface = {
    .name = "x86_adapt",
    .init_device = freq_gen_x86a_init_uncore_device,
    .get_num_devices = freq_gen_x86a_get_max_cpus_uncore,
    .prepare_set_frequency = freq_gen_x86a_prepare_access_uncore,
    .get_frequency = freq_gen_x86_get_frequency_uncore,
    .get_min_frequency = freq_gen_x86_get_min_frequency_uncore,
    .set_frequency = freq_gen_x86_set_frequency_uncore,
    .set_min_frequency = freq_gen_x86_set_min_frequency_uncore,
    .unprepare_set_frequency = freq_gen_x86a_unprepare_access_uncore,
    .close_device = freq_gen_x86a_close_file_uncore,
    .finalize = freq_gen_x86a_finalize_uncore
};

static freq_gen_interface_t* freq_gen_x86a_init_uncorefreq(void)
{
    int ret = freq_gen_x86a_init_uncore();
    if (ret)
    {
        errno = ret;
        LIBFREQGEN_SET_ERROR("could not initialize x86a/uncore");
        return NULL;
    }
    return &freq_gen_x86a_uncore_interface;
}

freq_gen_interface_internal_t freq_gen_x86a_interface_internal = {
    .name = "x86_adapt",
    .init_cpufreq = freq_gen_x86a_init_cpufreq,
    .init_uncorefreq = freq_gen_x86a_init_uncorefreq
};
