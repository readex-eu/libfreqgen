/*
 * freq_gen_internal.h
 *
 *  Created on: 26.01.2018
 *      Author: rschoene
 */

#ifndef SRC_FREQ_GEN_INTERNAL_H_
#define SRC_FREQ_GEN_INTERNAL_H_

#include "../include/freq_gen.h"

#define  BUFFER_SIZE 4096

typedef struct{
	freq_gen_interface_t * (*init_cpufreq)( void );
	freq_gen_interface_t * (*init_uncorefreq)( void );
} freq_gen_interface_internal_t;

extern freq_gen_interface_internal_t freq_gen_sysfs_interface_internal;
extern freq_gen_interface_internal_t freq_gen_x86a_interface_internal;
extern freq_gen_interface_internal_t freq_gen_msr_interface_internal;
extern freq_gen_interface_internal_t freq_gen_likwid_interface_internal;

#endif /* SRC_FREQ_GEN_INTERNAL_H_ */
