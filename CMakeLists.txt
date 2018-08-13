cmake_minimum_required(VERSION 3.1)
project(freqgen)

option(GIT_UPDATE_SUBMODULES "Automatically update git submodules during CMake run" ON)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Debug' as none was specified.")
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)

    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
endif()


SET(SOURCES src/sysfs.c src/msr-safe.c src/freq_gen_internal_generic.c src/freq_gen.c)

find_package(X86Adapt)

if (X86Adapt_FOUND)
    include_directories(${X86_ADAPT_INCLUDE_DIRS})
    LIST(APPEND SOURCES src/x86_adapt.c)
    SET( CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -DUSEX86_ADAPT" )
endif()

find_package(Likwid)

if (LIKWID_FOUND)
    include_directories(${LIKWID_INCLUDE_DIRS})
    LIST(APPEND SOURCES src/likwid.c)
    SET(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -DUSELIKWID _DLIKWID_DEBUG" )
endif()

include_directories(include)
add_library(freqgen SHARED ${SOURCES})
set_target_properties(freqgen PROPERTIES PUBLIC_HEADER "include/freq_gen.h;include/freqgen.h")
target_compile_features(freqgen PUBLIC c_std_11)

if (X86Adapt_FOUND)
    target_link_libraries(freqgen ${X86_ADAPT_LIBRARIES})
endif()
if (LIKWID_FOUND)
    target_link_libraries(freqgen ${LIKWID_LIBRARIES})
endif()

install(TARGETS freqgen LIBRARY DESTINATION lib
        PUBLIC_HEADER DESTINATION include
)
