# libfreqgen
A library to access core and uncore frequencies of x86 processors that provides callbacks for msr-safe, x86-adapt, likwid, and sysfs entries
# X86-Energy Libraries for Score-P

## Compilation and Installation

### Prerequisites

To compile this plugin, you need:

* GCC compiler

* CMake 3.9+

### Build Options

* `CMAKE_INSTALL_PREFIX` (default `/usr/local`)
    
  Installation directory
    
* `CMAKE_BUILD_TYPE` (default `Debug`)
   
   Build type with different compiler options, can be `Debug` `Release` `MinSizeRel` `RelWithDebInfo`

*  `X86_ADAPT_LIBRARIES`
    
  Libraries for x86\_adapt, e.g., `-DX86_ADAPT_LIBRARIES=/opt/x86_adapt/lib/libx86_adapt_static.a`

*  `X86_ADAPT_INCLUDE_DIRS`
    
  Include directories for x86\_adapt, e.g., `-DX86_ADAPT_INCLUDE_DIRS=/opt/x86_adapt/include`

*  `LIKWID_LIBRARIES`
    
  Libraries for likwid, e.g.`-DLIKWID_LIBRARIES=/opt/likwi/lib/liblikwid.so`

*  `LIKWID_INCLUDE_DIRS`
    
    Include directories for likwid, e.g.`-DLIKWID_INCLUDE_DIRS=/opt/likwid/include`

* `X86A_STATIC` (default on)

  Link `x86_adapt` statically, if it is found

### Building

1. Create build directory

        mkdir build
        cd build

2. Invoking CMake

        cmake .. (options)

3. Invoking make

        make
        
4. Install

        make install


5. Add the installation path to `LD_LIBRARY_PATH` with

        export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`

## Usage

See the documentation in 

During runtime, the library will try to access the following interfaces:

1. msr and msr-safe, provided by the `msr` and `msr-safe` kernel module, available for Intel processors
2. x86-adapt, provided by the `x86_adapt` kernel module (if found during installation), available for Intel processors
3. likwid, provided by `likwid` the `cpufreq` kernel module (if found during installation)
4. sysfs cpufreq, provided by the `cpufreq` kernel module. Only available for CPU frequency, but not for uncore frequency. Make sure that `intel_pstate` is deactivated if you want to use this option. Furthermore, check whether you can read and write 
- `/sys/devices/system/cpu/cpu<nr>/cpufreq/scaling_setspeed`
Also make sure that 
- `/sys/devices/system/cpu/cpu<nr>/cpufreq/scaling_governor`
is set to userspace

## Enforce a specific interface

You can enforce a specific interface by setting the environment variable `LIBFREQGEN_CORE_INTERFACE` and `LIBFREQGEN_UNCORE_INTERFACE` to one of these values:

 - `likwid` selects likwid
 - `msr` selects access via msr/msr-safe
 - `sysfs` selects cpufreq sysfs entries (not for `LIBFREQGEN_UNCORE_INTERFACE`)
 - `x86_adapt` selects x86_adapt

### If anything fails

1. Check whether the libraries can be loaded from the `LD_LIBRARY_PATH`.

2. Write a mail to the author.

## Authors

* Robert Schoene (robert.schoene at tu-dresden dot de)
* Andreas Gocht (andreas.gocht at tu-dresden dot de)
* Umbreen Sabir Mian (umbreen.mian at tu-dresden dot de)