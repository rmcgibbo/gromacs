#
# This file is part of the GROMACS molecular simulation package.
#
# Copyright (c) 2012,2013,2014,215, by the GROMACS development team, led by
# Mark Abraham, David van der Spoel, Berk Hess, and Erik Lindahl,
# and including many others, as listed in the AUTHORS file in the
# top-level source directory and at http://www.gromacs.org.
#
# GROMACS is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 2.1
# of the License, or (at your option) any later version.
#
# GROMACS is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with GROMACS; if not, see
# http://www.gnu.org/licenses, or write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
#
# If you want to redistribute modifications to GROMACS, please
# consider that scientific software is very special. Version
# control is crucial - bugs must be traceable. We will be happy to
# consider code for inclusion in the official distribution, but
# derived work must not be called official GROMACS. Details are found
# in the README & COPYING files - if they are missing, get the
# official version at http://www.gromacs.org.
#
# To help us fund GROMACS development, we humbly ask that you cite
# the research papers on the package. Check out http://www.gromacs.org.

# Manage CUDA nvcc compilation configuration, try to be smart to ease the users'
# pain as much as possible:
# - use the CUDA_HOST_COMPILER if defined by the user, otherwise
# - auto-detect compatible nvcc host compiler and set nvcc -ccbin (if not MPI wrapper)
# - set icc compatibility mode to gcc 4.6
# - (advanced) variables set:
#   * CUDA_HOST_COMPILER            - the host compiler for nvcc (only with cmake <2.8.10)
#   * CUDA_HOST_COMPILER_OPTIONS    - the full host-compiler related option list passed to nvcc
#
# Note that from CMake 2.8.10 FindCUDA defines CUDA_HOST_COMPILER internally,
# so we won't set it ourselves, but hope that the module does a good job.

gmx_check_if_changed(CUDA_HOST_COMPILER_CHANGED CUDA_HOST_COMPILER)

# CUDA_HOST_COMPILER changed hence it is not auto-set anymore
if (CUDA_HOST_COMPILER_CHANGED AND CUDA_HOST_COMPILER_AUTOSET)
    unset(CUDA_HOST_COMPILER_AUTOSET CACHE)
endif()

# Explicitly set the host compiler for nvcc if the current compiler is
# supported and it's not an MPI compiler wrapper, otherwise warn the user.
#
# Note that even though nvcc compiles host code as C++, we use the
# CMAKE_C_COMPILER as host compiler. We do this because CUDA versions
# preceding 5.0 only recognize icc, but not icpc. However, both gcc and icc
# (i.e. all supported compilers) happily compile C++ code.
#
# Also note that with MSVC nvcc sets the -compiler-bindir option behind the
# scenes; to avoid conflicts we don't set -ccbin automatically.
#
# This will be executed only with cmake <v2.8.10 as later versions set the
# host compiler in FindCUDA.
if (NOT DEFINED CUDA_HOST_COMPILER AND NOT MSVC)
    if (NOT CMAKE_COMPILER_IS_GNUCC AND
        NOT (CMAKE_C_COMPILER_ID MATCHES "Intel" AND UNIX AND NOT APPLE))
        message(WARNING "
        Will not set the nvcc host compiler because the current C compiler is not
        compatible with nvcc:
        ${CMAKE_C_COMPILER} (ID: ${CMAKE_C_COMPILER_ID})
        Compatible compilers are: gcc on Linux and Mac OS X, the Intel Compiler on 64-bit
        Linux and MSVC on Windows. Note that with newer CUDA releases this might change,
        for up-to-date compatibility information check the NVIDIA documentation.
        If nothing specified, nvcc will automatically pick the platform-default compiler;
        Note that mixing compilers can cause errors.
        To manually set the nvcc host compiler, edit CUDA_NVCC_FLAGS or re-configure
        setting CUDA_HOST_COMPILER to the full path of a compatible compiler.
        ")
    else()
        # do not use MPI compiler wrappers, as these are prone to brake nvcc
        if (GMX_MPI AND NOT "${MPI_C_FOUND}") # FindMPI-based detection
            message(WARNING "
        Will not set the nvcc host compiler because the current C compiler is an MPI
        compiler wrapper: ${CMAKE_C_COMPILER}
        MPI compiler wrappers are prone to not work with nvcc. You might get lucky,
        but the safest is to use the C compiler that the MPI compiler wrapper uses
        (if this is compatible).
        To manually set the nvcc host compiler, edit CUDA_NVCC_FLAGS or re-configure
        setting CUDA_HOST_COMPILER to the full path of a compatible compiler.
        ")
        else()
            set(CUDA_HOST_COMPILER "${CMAKE_C_COMPILER}")
            set(CUDA_HOST_COMPILER_AUTOSET TRUE CACHE INTERNAL
                "True if CUDA_HOST_COMPILER is automatically set")
        endif()
    endif()
endif()

# set up host compiler and its options
if(CUDA_HOST_COMPILER_CHANGED)
    # FindCUDA in CMake 2.8.10 sets the host compiler internally
    if (CMAKE_VERSION VERSION_LESS "2.8.10")
        message(STATUS "Setting the nvcc host compiler to: ${CUDA_HOST_COMPILER}")
        set(CUDA_HOST_COMPILER ${CUDA_HOST_COMPILER}
            CACHE PATH "Host compiler for nvcc")
    endif()

    # On *nix force icc in gcc 4.6 compatibility mode. This is needed
    # as even with icc used as host compiler, when icc's gcc compatibility
    # mode is higher than the max gcc version officially supported by CUDA,
    # nvcc will freak out.
    set(CUDA_HOST_COMPILER_OPTIONS "")
    if (UNIX AND
            ((CMAKE_C_COMPILER_ID MATCHES "Intel" AND
              (CUDA_HOST_COMPILER_AUTOSET OR CMAKE_C_COMPILER STREQUAL CUDA_HOST_COMPILER)) OR
            (CMAKE_CXX_COMPILER_ID MATCHES "Intel" AND CMAKE_CXX_COMPILER STREQUAL CUDA_HOST_COMPILER))
        )
        message(STATUS "Setting Intel Compiler compatibity mode to gcc 4.6 for nvcc host compilation")
        list(APPEND CUDA_HOST_COMPILER_OPTIONS "-Xcompiler;-gcc-version=460")
    endif()

    if(APPLE AND CMAKE_C_COMPILER_ID MATCHES "GNU")
        # Some versions of gcc-4.8 and gcc-4.9 produce errors (in particular on OS X)
        # if we do not use -D__STRICT_ANSI__. It is harmless, so we might as well add it for all versions.
        list(APPEND CUDA_HOST_COMPILER_OPTIONS "-D__STRICT_ANSI__")
    endif()

    set(CUDA_HOST_COMPILER_OPTIONS "${CUDA_HOST_COMPILER_OPTIONS}"
        CACHE STRING "Options for nvcc host compiler (do not edit!).")

    mark_as_advanced(CUDA_HOST_COMPILER CUDA_HOST_COMPILER_OPTIONS)
endif()

# If any of these manual override variables for target CUDA GPU architectures
# or virtual architecture is set, parse the values and assemble the nvcc
# command line for these. Otherwise use our defaults.
# Note that the manual override variables require a semicolon separated
# architectures codes.
if (GMX_CUDA_TARGET_SM OR GMX_CUDA_TARGET_COMPUTE)
    set(GMX_CUDA_NVCC_GENCODE_FLAGS)
    set(_target_sm_list ${GMX_CUDA_TARGET_SM})
    foreach(_target ${_target_sm_list})
        list(APPEND GMX_CUDA_NVCC_GENCODE_FLAGS "-gencode;arch=compute_${_target},code=sm_${_target}")
    endforeach()
    set(_target_compute_list ${GMX_CUDA_TARGET_COMPUTE})
    foreach(_target ${_target_compute_list})
        list(APPEND GMX_CUDA_NVCC_GENCODE_FLAGS "-gencode;arch=compute_${_target},code=compute_${_target}")
    endforeach()
else()
    # Set the CUDA GPU architectures to compile for:
    # - with CUDA >=5.0 <6.5:   CC <=3.5 is supported
    #     => compile sm_20, sm_30, sm_35 cubin, and compute_35 PTX
    # - with CUDA >=6.5:        CC <=3.7 and 5.0 are supported
    #     => compile sm_20, sm_30, sm_35, sm_37 sm_50, cubin, and compute_50 PTX
    #
    #   Note that CUDA 6.5.19 second patch release supports cc 5.2 too, but
    #   CUDA_VERSION does not contain patch version and having PTX 5.0 JIT-ed is
    #   equally fast as compiling with sm_5.2 anyway.
    list (APPEND GMX_CUDA_NVCC_GENCODE_FLAGS "-gencode;arch=compute_20,code=sm_20")
    list (APPEND GMX_CUDA_NVCC_GENCODE_FLAGS "-gencode;arch=compute_30,code=sm_30")
    list (APPEND GMX_CUDA_NVCC_GENCODE_FLAGS "-gencode;arch=compute_35,code=sm_35")
    if(CUDA_VERSION VERSION_GREATER "6.4999") # >= 6.5
        list (APPEND GMX_CUDA_NVCC_GENCODE_FLAGS "-gencode;arch=compute_37,code=sm_37")
        list (APPEND GMX_CUDA_NVCC_GENCODE_FLAGS "-gencode;arch=compute_50,code=sm_50")
    endif()

    if(CUDA_VERSION VERSION_LESS "6.5")
        list (APPEND GMX_CUDA_NVCC_GENCODE_FLAGS "-gencode;arch=compute_35,code=compute_35")
    else() # version >= 6.5
        list (APPEND GMX_CUDA_NVCC_GENCODE_FLAGS "-gencode;arch=compute_50,code=compute_50")
    endif()
endif()

gmx_dependent_cache_variable(GMX_CUDA_TARGET_SM "List of CUDA GPU architecture codes to compile for (without the sm_ prefix)" STRING "" GMX_CUDA_TARGET_SM)
gmx_dependent_cache_variable(GMX_CUDA_TARGET_COMPUTE "List of CUDA virtual architecture codes to compile for (without the compute_ prefix)" STRING "" GMX_CUDA_TARGET_COMPUTE)

# assemble the CUDA flags
list(APPEND GMX_CUDA_NVCC_FLAGS "${GMX_CUDA_NVCC_GENCODE_FLAGS}")
list(APPEND GMX_CUDA_NVCC_FLAGS "-use_fast_math")

# assemble the CUDA host compiler flags
# with CMake <2.8.10 the host compiler needs to be set on the nvcc command line
if (CMAKE_VERSION VERSION_LESS "2.8.10")
    list(APPEND GMX_CUDA_NVCC_FLAGS "-ccbin=${CUDA_HOST_COMPILER}")
endif()
list(APPEND GMX_CUDA_NVCC_FLAGS "${CUDA_HOST_COMPILER_OPTIONS}")

# The flags are set as local variables which shadow the cache variables. The cache variables
# (can be set by the user) are appended. This is done in a macro to set the flags when all
# host compiler flags are already set.
macro(GMX_SET_CUDA_NVCC_FLAGS)
    if(CUDA_PROPAGATE_HOST_FLAGS)
        set(CUDA_PROPAGATE_HOST_FLAGS OFF)

        # When CUDA 6.5 is required we should use C++11 also for CUDA and also propagate
        # the C++11 flag to CUDA. Then we can use the solution implemented in FindCUDA
        # (starting with 3.3 - can be backported). For now we need to remove the C++11
        # flag which means we need to manually propagate all other flags.
        string(REGEX REPLACE "[-]+std=c\\+\\+0x" "" _CMAKE_CXX_FLAGS_NOCXX11 "${CMAKE_CXX_FLAGS}")

        # The IBM xlc compiler chokes if we use both altivec and Cuda. Solve
        # this by not propagating the flag in this case.
        if(CMAKE_CXX_COMPILER_ID MATCHES "XL")
            string(REGEX REPLACE "-qaltivec" _CMAKE_CXX_FLAGS_NOCXX11 "$_CMAKE_CXX_FLAGS_NOCXX11")
        endif()

        string(REPLACE " " "," _flags "${_CMAKE_CXX_FLAGS_NOCXX11}")
        set(CUDA_NVCC_FLAGS "${GMX_CUDA_NVCC_FLAGS};${CUDA_NVCC_FLAGS};-Xcompiler;${_flags}")

        # Create list of all possible configurations. For multi-configuration this is CMAKE_CONFIGURATION_TYPES
        # and for single configuration CMAKE_BUILD_TYPE. Not sure why to add the default ones, but FindCUDA
        # claims one should.
        set(CUDA_configuration_types ${CMAKE_CONFIGURATION_TYPES} ${CMAKE_BUILD_TYPE} Debug MinSizeRel Release RelWithDebInfo)
        list(REMOVE_DUPLICATES CUDA_configuration_types)

        foreach(_config ${CUDA_configuration_types})
            string(TOUPPER ${_config} _config_upper)
            string(REPLACE " " "," _flags "${CMAKE_CXX_FLAGS_${_config_upper}}")
            set(CUDA_NVCC_FLAGS_${_config_upper} "${CUDA_NVCC_FLAGS_${_config_upper}};-Xcompiler;${_flags}")
        endforeach()
    else()
        set(CUDA_NVCC_FLAGS "${GMX_CUDA_NVCC_FLAGS};${CUDA_NVCC_FLAGS}")
    endif()
endmacro()
