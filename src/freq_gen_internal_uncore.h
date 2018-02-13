/*
 * freq_gen_internal_uncore.h
 *
 *  Created on: 13.02.2018
 *      Author: rschoene
 */

#ifndef SRC_FREQ_GEN_INTERNAL_UNCORE_H_
#define SRC_FREQ_GEN_INTERNAL_UNCORE_H_

/*
 * will return the max nr from /sys/devices/system/node/node(nr)
 * will fail on sysfs not accessible
 * */
int freq_gen_get_num_uncore( void );


#endif /* SRC_FREQ_GEN_INTERNAL_UNCORE_H_ */
