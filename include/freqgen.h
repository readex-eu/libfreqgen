/*
 * freqgen.h
 *
 *  Created on: 20.03.2018
 *      Author: rschoene
 */

#ifndef SRC_FREQGEN_INTERFACE_H_
#define SRC_FREQGEN_INTERFACE_H_

typedef enum
{
	FREQ_GEN_DEVICE_CORE_FREQ,
	FREQ_GEN_DEVICE_UNCORE_FREQ,
	FREQ_GEN_DEVICE_NUM
} freq_gen_dev_type;

typedef void * freq_gen_setting_t;
typedef int freq_gen_single_device_t;

typedef struct
{
	char * name;
	int (*get_num_devices)(); /** < returns the number of available devices (e.g., number of CPUs, uncores) */
	/**
	 * initialize a specific device, should be done once per cpu/uncore before using it
	 * @param cpu_nr the CPU number or uncore number
	 * @return a file descriptor (int) , otherwise -ERRNO
	 */
	freq_gen_single_device_t (*init_device)(int cpu_nr);
	/**
	 * prepare a specific frequency for a device
	 * @param frequency the frequency to set in Hz
	 * @param turbo should turbo be enabled? (currently ignored)
	 * @return NULL if failed, otherwise a pointer
	 */
	freq_gen_setting_t (*prepare_set_frequency)(long long int target,int turbo);/** < prepare a specific frequency for a device */

	/**
	 * get the frequency of a core/uncore
	 * If the interface provides a frequency range, it will return the maximal frequency
	 * @param fp from init_device
	 * @return frequency in Hz or an error (<0)
	 */
	long long int (*get_frequency)(freq_gen_single_device_t fp);

    /**
     * get the minimal frequency of a core/uncore
     * If the interface provides a frequency range, it will return the maximal frequency
     * Callers must check whether this function exists (get_min_frequency == NULL) before using it.
     * If it does not exist, the interface is either not there or not implemented and the frequency is not considered to be a range.
     * @param fp from init_device
     * @return frequency in Hz or an error (<0)
     */
    long long int (*get_min_frequency)(freq_gen_single_device_t fp);

	/**
	 * set the frequency on a core/uncore
     * If the interface provides a frequency range, it will set minimal and maximal frequency
	 * @param fp from init_device
	 * @param target from prepare_set_frequency
	 * @return 0 or an error defined in errno.h
	 */
	int (*set_frequency)(freq_gen_single_device_t fp, freq_gen_setting_t target);

    /**
     * set the minimal frequency on a core/uncore
     * Callers must check whether this function exists (set_min_frequency == NULL) before using it.
     * If it does not exist, the interface is either not there or not implemented and the frequency is not considered to be a range.
     * @param fp from init_device
     * @param target from prepare_set_frequency
     * @return 0 or an error defined in errno.h
     */
    int (*set_min_frequency)(freq_gen_single_device_t fp, freq_gen_setting_t target);

	/**
	 * free resources of a setting
	 * @param setting a setting created with prepare_set_frequency
	 */
	void (*unprepare_set_frequency)(freq_gen_setting_t setting);
	/**
	 * close a single device
	 * @param cpu_nr the cpu_nr used in init_device() that resulted in the fp
	 * @param fp the handle to close the device
	 */
	void (*close_device)(int cpu_nr, freq_gen_single_device_t fp);

	/**
	 * finalize the interface
	 */
	void (*finalize)();
} freq_gen_interface_t;

/**
 * Will return a method for accessing either uncore or core frequency, based on type
 * @param type get a handle for core or uncore frequency. The function will iterate over some implementations and return the first suitable.
 * You may try to open a core/uncore device to check whether this works immediately after. If not, you can finalize the interface and get the next one.
 * When there are no more suitable interfaces, this function returns NULL. When calling it again after receiving NULL, it will also start trying out the interface again from the beginning.
 * @return a handle to functions to use from now on
 */
freq_gen_interface_t * freq_gen_init(freq_gen_dev_type type);


#endif /* SRC_FREQGEN_INTERFACE_H_ */
