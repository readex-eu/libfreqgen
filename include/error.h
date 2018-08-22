/*
 * error.h
 *
 *  Created on: 23.07.2018
 *      Author: jitschin
 */

#ifndef SRC_ERROR_H_
#define SRC_ERROR_H_

#define LIBFREQGEN_SET_ERROR(...)                                                                  \
    freq_gen_set_error_string(__FILE__, __func__, __LINE__, __VA_ARGS__)
#define LIBFREQGEN_APPEND_ERROR(...)                                                               \
    freq_gen_append_error_string(__FILE__, __func__, __LINE__, __VA_ARGS__)

void freq_gen_set_error_string(const char* error_file, const char* error_func, int error_line,
                                 const char* fmt, ...);
void freq_gen_append_error_string(const char* error_file, const char* error_func, int error_line,
                                    const char* fmt, ...);

#endif /* SRC_ERROR_H_ */
