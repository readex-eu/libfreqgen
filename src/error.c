/*
 * error.c
 *
 *  Created on: 23.07.2018
 *      Author: jitschin
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "error.h"

#define ERROR_LEN 4096

static char error_string[4096]={'\0'};

static char * not_enough_space="Not enough space for writing error\n";
static char * internal_error="Error while writing error (errorception)\n";


char * libfreqgen_error_string( void )
{
    return error_string;
}

static int libfreqgen_handle_problematic_error_string( const int printf_return);
static int libfreqgen_append_newline( char * strPtr, const int maxlen);

void libfreqgen_set_error_string(const char * error_file, const char * error_func, int error_line, const char * fmt, ... )
{
    va_list valist;
    if(! libfreqgen_handle_problematic_error_string(snprintf(error_string, ERROR_LEN, "Error in function %s at %s:%d: ", error_func, error_file, error_line)))
    {
        va_start(valist, fmt);
    	libfreqgen_handle_problematic_error_string(vsnprintf(&error_string[strlen(error_string)], ERROR_LEN - strlen(error_string), fmt, valist));
        va_end(valist);
        libfreqgen_append_newline(error_string, ERROR_LEN);
    }
    return;
}
void libfreqgen_append_error_string(const char * error_file, const char * error_func, int error_line, const char * fmt, ... )
{
    va_list valist;
    if(! libfreqgen_handle_problematic_error_string(snprintf(&error_string[strlen(error_string)], ERROR_LEN - strlen(error_string), "Error in function %s at %s:%d: ", error_func, error_file, error_line)))
    {
        va_start(valist, fmt);
    	libfreqgen_handle_problematic_error_string(vsnprintf(&error_string[strlen(error_string)], ERROR_LEN - strlen(error_string), fmt, valist));
        va_end(valist);
        libfreqgen_append_newline(error_string, ERROR_LEN);
    }
    return;
}
static int libfreqgen_handle_problematic_error_string( const int printf_return)
{
    if ( printf_return+strlen( error_string ) >= 4096 )
    {
        sprintf(error_string,"%s",not_enough_space);
        return -1;
    }
    if ( printf_return < 0 )
    {
        sprintf(error_string,"%s",internal_error);
        return -2;
    }

    return 0;
}

static int libfreqgen_append_newline(char * strPtr, const int maxlen)
{
    int old_len = strlen(strPtr);
    if(old_len + 2 < maxlen)
    {
      strPtr[old_len] = '\n';
      strPtr[old_len + 1] = '\0';
    } else
    {
    	return -1;
    }

    return 0;
}


