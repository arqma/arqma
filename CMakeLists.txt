# Copyright (c) 2018-2022, The Arqma Network
# Copyright (c) 2014-2018, The Monero Project
#
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification, are
# permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this list of
#    conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice, this list
#    of conditions and the following disclaimer in the documentation and/or other
#    materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its contributors may be
#    used to endorse or promote products derived from this software without specific
#    prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
# THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

if(NOT WIN32)
  string(ASCII 27 Esc)
  set(ColourReset "${Esc}[m")
  set(ColourBold  "${Esc}[1m")
  set(Red         "${Esc}[31m")
  set(Green       "${Esc}[32m")
  set(Yellow      "${Esc}[33m")
  set(Blue        "${Esc}[34m")
  set(Magenta     "${Esc}[35m")
  set(Cyan        "${Esc}[36m")
  set(White       "${Esc}[37m")
  set(BoldRed     "${Esc}[1;31m")
  set(BoldGreen   "${Esc}[1;32m")
  set(BoldYellow  "${Esc}[1;33m")
  set(BoldBlue    "${Esc}[1;34m")
  set(BoldMagenta "${Esc}[1;35m")
  set(BoldCyan    "${Esc}[1;36m")
  set(BoldWhite   "${Esc}[1;37m")
endif()

# Above I did add some colours that can be used at Unix-based Systems :)

function(die msg)
  if(WIN32)
    set(ColourReset  "")
    set(BoldRed    "")
  endif()

  message(FATAL_ERROR "${BoldRed}${msg}${ColourReset}")
endfunction()

function(statusinfo msg)
  if(WIN32)
    set(ColourReset  "")
    set(BoldCyan   "")
  endif()

  message(STATUS "${BoldCyan}${msg}${ColourReset}")
endfunction()

function(warn msg)
  if(WIN32)
    set(ColourReset  "")
    set(BoldMagenta "")
  endif()

  message(WARNING "${BoldMagenta}${msg}${ColourReset}")
endfunction()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(INSERT CMAKE_MODULE_PATH 0
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
include(CheckLinkerFlag)
include(CheckLibraryExists)
include(CheckFunctionExists)

if(IOS)
  include(CMakeLists_IOS.txt)
endif()

cmake_minimum_required(VERSION 3.12...3.31)
statusinfo("CMake version ${CMAKE_VERSION}")

project(arqma
  LANGUAGES CXX C)

if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

# On Darwin, ensure the user-defined paths are used to find libs
# before falling back to the system-defined.
set(CMAKE_FIND_FRAMEWORK "LAST")

option(USE_CCACHE "Use ccache if found" ON)
if(USE_CCACHE)
  include(FindCcache)
else()
  statusinfo("ccache not used")
endif()
option (USE_COMPILATION_TIME_PROFILER "Use compilation time profiler (for CLang >= 9 only)" OFF)
if(USE_COMPILATION_TIME_PROFILER)
  if (NOT "${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    message(FATAL_ERROR "The flag USE_COMPILATION_TIME_PROFILER is meant to be set only for CLang compiler!")
  endif()
  add_compile_options("-ftime-trace")
endif()

enable_language(C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

function (add_c_flag_if_supported flag var)
  string(REPLACE "-" "_" supported ${flag}_c)
  check_c_compiler_flag(${flag} ${supported})
  if(${${supported}})
    set(${var} "${${var}} ${flag}" PARENT_SCOPE)
  endif()
endfunction()

function (add_cxx_flag_if_supported flag var)
  string(REPLACE "-" "_" supported ${flag}_cxx)
  check_cxx_compiler_flag(${flag} ${supported})
  if(${${supported}})
    set(${var} "${${var}} ${flag}" PARENT_SCOPE)
  endif()
endfunction()

function (add_linker_flag_if_supported flag var)
  string(REPLACE "-" "_" supported ${flag}_ld)
  string(REPLACE "," "_" supported ${flag}_ld)
  check_linker_flag(${flag} ${supported})
  if(${${supported}})
    set(${var} "${${var}} ${flag}" PARENT_SCOPE)
  endif()
endfunction()

function (add_definition_if_function_found function var)
  string(REPLACE "-" "_" supported ${function}_function)
  check_function_exists(${function} ${supported})
  if(${${supported}})
    add_definitions("-D${var}")
  endif()
endfunction()

function (add_definition_if_library_exists library function header var)
  string(REPLACE "-" "_" supported ${function}_library)
  check_library_exists(${library} ${function} ${header} ${supported})
  if(${${supported}})
    add_definitions("-D${var}")
  endif()
endfunction()

# Properly links a target to a list of library names by finding the given libraries.  Takes:
# - a target
# - a linktype (e.g. INTERFACE, PUBLIC, PRIVATE)
# - a library search path (or "" for defaults)
# - any number of library names
function(link_dep_libs target linktype libdirs)
  foreach(lib ${ARGN})
    find_library(link_lib-${lib} NAMES ${lib} PATHS ${libdirs})
    if(link_lib-${lib})
      target_link_libraries(${target} ${linktype} ${link_lib-${lib}})
    endif()
  endforeach()
endfunction()

option(RELINK_TARGETS "Relink targets, when just a dependant .so changed, but not its header?" OFF)
function(arqma_set_target_no_relink target)
  if(RELINK_TARGETS MATCHES OFF)
    set_target_properties("${target}" PROPERTIES LINK_DEPENDS_NO_SHARED true)
  endif()
endfunction()

option(STRIP_TARGETS "Strip symbols from targets?" OFF)
function(arqma_set_target_strip target)
  if(STRIP_TARGETS)
    set_target_properties("${target}" PROPERTIES LINK_FLAGS_RELEASE -s)
    set_target_properties("${target}" PROPERTIES LINK_FLAGS_DEBUG -s)
  endif()
endfunction()

function(arqma_add_minimal_executable name)
  source_group("${name}" FILES ${ARGN})
  add_executable("${name}" ${ARGN})
    arqma_set_target_no_relink("${name}")
    arqma_set_target_strip("${name}")
endfunction()

if(MINGW)
  function(export_all_symbols)
    unset(TMP)
    add_linker_flag_if_supported(-Wl,--export-all-symbols TMP)
    string(APPEND CMAKE_SHARED_LINKER_FLAGS ${TMP})
    string(APPEND CMAKE_MODULE_LINKER_FLAGS ${TMP})
    set(CMAKE_SHARED_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS} PARENT_SCOPE)
    set(CMAKE_MODULE_LINKER_FLAGS ${CMAKE_MODULE_LINKER_FLAGS} PARENT_SCOPE)
  endfunction()
  export_all_symbols()
endif()

macro (arqma_find_all_headers headers_found module_root_dir)
  file(GLOB ${headers_found}
           "${module_root_dir}/*.h*"
           "${module_root_dir}/**/*.h*"
           "${module_root_dir}/*.inl"
           "${module_root_dir}/**/*.inl"
  )
endmacro()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
  statusinfo("Setting default build type: ${CMAKE_BUILD_TYPE}")
endif()

# ARCH defines the target architecture, either by an explicit identifier or
# one of the following two keywords. By default, ARCH a value of 'native':
# target arch = host arch, binary is not portable. When ARCH is set to the
# string 'default', no -march arg is passed, which creates a binary that is
# portable across processors in the same family as host processor.  In cases
# when ARCH is not set to an explicit identifier, cmake's builtin is used
# to identify the target architecture, to direct logic in this cmake script.
# Since ARCH is a cached variable, it will not be set on first cmake invocation.
if(NOT ARCH_ID)
  if(NOT ARCH OR ARCH STREQUAL "" OR ARCH STREQUAL "native" OR ARCH STREQUAL "default")
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "")
      set(CMAKE_SYSTEM_PROCESSOR ${CMAKE_HOST_SYSTEM_PROCESSOR})
    endif()
    set(ARCH_ID "${CMAKE_SYSTEM_PROCESSOR}")
  else()
    set(ARCH_ID "${ARCH}")
  endif()
endif()
string(TOLOWER "${ARCH_ID}" ARM_ID)
string(SUBSTRING "${ARM_ID}" 0 3 ARM_TEST)
if(ARM_TEST STREQUAL "arm")
  set(ARM 1)
  string(SUBSTRING "${ARM_ID}" 0 5 ARM_TEST)
  if(ARM_TEST STREQUAL "armv6")
    set(ARM6 1)
  endif()
  if(ARM_TEST STREQUAL "armv7")
    set(ARM7 1)
  endif()
endif()

if(ARM_ID STREQUAL "aarch64" OR ARM_ID STREQUAL "arm64" OR ARM_ID STREQUAL "armv8-a")
  set(ARM 1)
  set(ARM8 1)
  set(ARCH "armv8-a")
endif()

if(ARCH_ID STREQUAL "ppc64le")
  set(PPC64LE 1)
  set(PPC64   0)
  set(PPC     0)
endif()

if(ARCH_ID STREQUAL "powerpc64" OR ARCH_ID STREQUAL "ppc64")
  set(PPC64LE 0)
  set(PPC64   1)
  set(PPC     0)
endif()

if(ARCH_ID STREQUAL "powerpc" OR ARCH_ID STREQUAL "ppc")
  set(PPC64LE 0)
  set(PPC64   0)
  set(PPC     1)
endif()

if(ARCH_ID STREQUAL "s390x")
  set(S390X 1)
endif()

if(WIN32 OR ARM OR PPC64LE OR PPC64 OR PPC)
  set(OPT_FLAGS_RELEASE "-O2")
else()
  set(OPT_FLAGS_RELEASE "-Ofast")
endif()

# BUILD_TAG is used to select the build type to check for a new version
if(BUILD_TAG)
  statusinfo("Building build tag ${BUILD_TAG}")
  add_definitions("-DBUILD_TAG=${BUILD_TAG}")
else()
  statusinfo("Building without build tag")
endif()

if(NOT MANUAL_SUBMODULES)
  find_package(Git)
  if(GIT_FOUND)
    statusinfo("Initializing Arqma Submodules")
    execute_process(COMMAND git "submodule" "update" "--init" "--recursive" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    function (check_submodule relative_path)
      execute_process(COMMAND git rev-parse "HEAD" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${relative_path} OUTPUT_VARIABLE localHead)
      execute_process(COMMAND git rev-parse "HEAD:${relative_path}" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} OUTPUT_VARIABLE checkedHead)
      string(COMPARE EQUAL "${localHead}" "${checkedHead}" upToDate)
      if(upToDate)
        statusinfo("Submodule '${relative_path}' is up-to-date")
      else()
        statusinfo("${BoldRed}Submodule '${relative_path}' is not up-to-date. Forcing update!${ColourReset}")
        execute_process(COMMAND git "submodule" "update" "--init" "--force" ${relative_path})
      endif()
    endfunction ()

    statusinfo("Checking submodules")
    check_submodule(external/rapidjson)
    check_submodule(external/randomarq)
  endif()
endif()

set(CMAKE_C_FLAGS_RELEASE "-DNDEBUG ${OPT_FLAGS_RELEASE}")
set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG ${OPT_FLAGS_RELEASE}")

set(PER_BLOCK_CHECKPOINT 1)
add_definitions("-DPER_BLOCK_CHECKPOINT")
set(Blocks "blocks")

list(INSERT CMAKE_MODULE_PATH 0
  "${CMAKE_SOURCE_DIR}/cmake")

if(NOT DEFINED ENV{DEVELOPER_LOCAL_TOOLS})
  statusinfo("Could not find DEVELOPER_LOCAL_TOOLS in env (not required)")
  set(BOOST_IGNORE_SYSTEM_PATHS_DEFAULT OFF)
elseif("$ENV{DEVELOPER_LOCAL_TOOLS}" EQUAL 1)
  statusinfo("Found: env DEVELOPER_LOCAL_TOOLS = 1")
  set(BOOST_IGNORE_SYSTEM_PATHS_DEFAULT ON)
else()
  statusinfo("Found: env DEVELOPER_LOCAL_TOOLS = 0")
  set(BOOST_IGNORE_SYSTEM_PATHS_DEFAULT OFF)
endif()

statusinfo("BOOST_IGNORE_SYSTEM_PATHS defaults to ${BOOST_IGNORE_SYSTEM_PATHS_DEFAULT}")
option(BOOST_IGNORE_SYSTEM_PATHS "Ignore boost system paths for local boost installation" ${BOOST_IGNORE_SYSTEM_PATHS_DEFAULT})

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
enable_testing()

option(BUILD_DOCUMENTATION "Build the Doxygen documentation." ON)
option(BUILD_TESTS "Build tests." OFF)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(DEFAULT_BUILD_DEBUG_UTILITIES ON)
else()
  set(DEFAULT_BUILD_DEBUG_UTILITIES OFF)
endif()
option(BUILD_DEBUG_UTILITIES "Build debug utilities." DEFAULT_BUILD_DEBUG_UTILITIES)

# Check whether we're on a 32-bit or 64-bit system
if(CMAKE_SIZEOF_VOID_P EQUAL "8")
  set(DEFAULT_BUILD_64 ON)
else()
  set(DEFAULT_BUILD_64 OFF)
endif()
option(BUILD_64 "Build for 64-bit? 'OFF' builds for 32-bit." ${DEFAULT_BUILD_64})

if(BUILD_64)
  set(ARCH_WIDTH "64")
else()
  set(ARCH_WIDTH "32")
endif()
statusinfo("Building for a ${ARCH_WIDTH}-bit system")

# CMAKE_SYSTEM_NAME checks are commonly known, but specifically taken from libsdl's CMakeLists
if(CMAKE_SYSTEM_NAME MATCHES "kFreeBSD.*|FreeBSD")
  set(FREEBSD TRUE)
endif()

# Check if we're on DragonFly BSD. See the README.md for build instructions.
if(CMAKE_SYSTEM_NAME MATCHES "DragonFly.*")
  set(DRAGONFLY TRUE)
endif()

# Check if we're on OpenBSD. See the README.md for build instructions.
if(CMAKE_SYSTEM_NAME MATCHES "kOpenBSD.*|OpenBSD.*")
  set(OPENBSD TRUE)
endif()

# TODO: check bsdi, NetBSD, to see if they need the same FreeBSD changes
#
# elseif(CMAKE_SYSTEM_NAME MATCHES "kNetBSD.*|NetBSD.*")
#   set(NETBSD TRUE)
# elseif(CMAKE_SYSTEM_NAME MATCHES ".*BSDI.*")
#   set(BSDI TRUE)

include_directories(external/rapidjson/include external/easylogging++ src contrib/epee/include external/cppzmq external)

if(POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW)
endif()

if(MINGW)
  set(DEFAULT_STATIC true)
else()
  set(DEFAULT_STATIC false)
endif()
option(STATIC "Try to link libraries statically if possible" ${DEFAULT_STATIC})

set(BUILD_SHARED_LIBS_DEFAULT OFF)
if(NOT STATIC AND CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(BUILD_SHARED_LIBS ON)
endif()
option(BUILD_SHARED_LIBS "Build internal libraries as shared" ${BUILD_SHARED_LIBS_DEFAULT})

if(BUILD_SHARED_LIBS)
  statusinfo("Building internal libraries with position independent code")
  add_definitions("-DBUILD_SHARED_LIBS")
else()
  set(CMAKE_VISIBILITY_INLINES_HIDDEN ON CACHE BOOL "Hide inlines")
  set(CMAKE_CXX_VISIBILITY_PRESET hidden)
  statusinfo("Building internal libraries as static")
endif()
set(PIC_FLAG "-fPIC")

if(STATIC)
  set(CMAKE_FIND_LIBRARY_SUFFIXES .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DZMQ_STATIC")
endif()

if(MINGW)
  string(REGEX MATCH "^[^/]:/[^/]*" msys2_install_path "${CMAKE_C_COMPILER}")
  statusinfo("MSYS location: ${msys2_install_path}")
  set(CMAKE_INCLUDE_PATH "${msys2_install_path}/mingw${ARCH_WIDTH}/include")
  # This is necessary because otherwise CMake will make Boost libraries -lfoo
  # rather than a full path. Unfortunately, this makes the shared libraries get
  # linked due to a bug in CMake which misses putting -static flags around the
  # -lfoo arguments.
  set(DEFLIB ${msys2_install_path}/mingw${ARCH_WIDTH}/lib)
  list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_DIRECTORIES ${DEFLIB})
  list(REMOVE_ITEM CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES ${DEFLIB})
endif()

option(SANITIZE "Use ASAN memory sanitizer" OFF)
if(SANITIZE)
  statusinfo("Using ASAN")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
endif()

set(DATABASE lmdb)
statusinfo("Using LMDB as default DB type")
set(BLOCKCHAIN_DB DB_LMDB)
add_definitions("-DDEFAULT_DB_TYPE=\"lmdb\"")
add_definitions("-DBLOCKCHAIN_DB=${BLOCKCHAIN_DB}")

# Can't install hook in static build on OSX, because OSX linker does not support --wrap
# On ARM, having libunwind package (with .so's only) installed breaks static link.
# When possible, avoid stack tracing using libunwind in favor of using easylogging++.

if(APPLE)
  set(DEFAULT_STACK_TRACE OFF)
  set(LIBUNWIND_LIBRARIES "")
elseif(DEPENDS AND NOT LINUX)
  set(DEFAULT_STACK_TRACE OFF)
  set(LIBUNWIND_LIBRARIES "")
elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU" AND NOT MINGW)
  set(DEFAULT_STACK_TRACE ON)
  set(STACK_TRACE_LIB "easylogging++") # for diag output only
  set(LIBUNWIND_LIBRARIES "")
elseif(ARM)
  set(DEFAULT_STACK_TRACE OFF)
  set(LIBUNWIND_LIBRARIES "")
else()
  find_package(Libunwind)
  if(LIBUNWIND_FOUND)
    set(DEFAULT_STACK_TRACE ON)
    set(STACK_TRACE_LIB "libunwind") # for diag output only
  else()
    set(DEFAULT_STACK_TRACE OFF)
    set(LIBUNWIND_LIBRARIES "")
  endif()
endif()

option(STACK_TRACE "Install a hook that dumps stack on exception" ${DEFAULT_STACK_TRACE})

if(STACK_TRACE)
  statusinfo("Stack trace on exception enabled (using ${STACK_TRACE_LIB})")
else()
  statusinfo("Stack trace on exception disabled")
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads)

if(CMAKE_SYSTEM_NAME MATCHES "(SunOS|Solaris)")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthreads")
endif()

if(APPLE AND NOT IOS)
  if(NOT OPENSSL_ROOT_DIR)
      EXECUTE_PROCESS(COMMAND brew --prefix openssl
        OUTPUT_VARIABLE OPENSSL_ROOT_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    statusinfo("Using OpenSSL found at ${OPENSSL_ROOT_DIR}")
  endif()
endif()

find_package(OpenSSL REQUIRED)
statusinfo("Using OpenSSL include dir at ${OPENSSL_INCLUDE_DIR}")
include_directories(${OPENSSL_INCLUDE_DIR})
if(STATIC AND NOT IOS)
  if(UNIX)
    set(OPENSSL_LIBRARIES "${OPENSSL_LIBRARIES};${CMAKE_DL_LIBS};${CMAKE_THREAD_LIBS_INIT}")
  elseif(MINGW)
    set(OPENSSL_LIBRARIES "${OPENSSL_LIBRARIES};${EXTRA_LIBRARIES}")
  endif()
endif()

if(WIN32)
  list(APPEND OPENSSL_LIBRARIES ws2_32 crypt32)
endif()

find_package(HIDAPI)

add_definition_if_library_exists(c memset_s "string.h" HAVE_MEMSET_S)
add_definition_if_library_exists(c explicit_bzero "strings.h" HAVE_EXPLICIT_BZERO)
add_definition_if_function_found(strptime HAVE_STRPTIME)

add_definitions(-DAUTO_INITIALIZE_EASYLOGGINGPP)

function(set_default_arch)
  if(OPENBSD)
    set(ARCH default)
  else()
    set(ARCH native)
  endif()

  set(ARCH ${ARCH} CACHE STRING "CPU to build for: -march value or 'default' to not pass -march at all")
endfunction()

if(NOT ARCH)
  set_default_arch()
endif()

option(COVERAGE "Enable profiling for test coverage report" OFF)
if(COVERAGE)
  statusinfo("Building with profiling for test coverage report")
endif()
macro(arqma_enable_coverage)
  if(COVERAGE)
    foreach(COV_FLAG -fprofile-arcs -ftest-coverage --coverage)
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COV_FLAG}")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COV_FLAG}")
    endforeach()
  endif()
endmacro()

function(arqma_add_library name)
  arqma_add_library_with_deps(NAME "${name}" SOURCES ${ARGN})
endfunction()

function(arqma_add_library_with_deps)
  cmake_parse_arguments(ARQMA_ADD_LIBRARY "" "NAME" "DEPENDS;SOURCES" ${ARGN})
  source_group("${ARQMA_ADD_LIBRARY_NAME}" FILES ${ARQMA_ADD_LIBRARY_SOURCES})

  set(objlib obj_${ARQMA_ADD_LIBRARY_NAME})
  add_library(${objlib} OBJECT ${ARQMA_ADD_LIBRARY_SOURCES})
  add_library("${ARQMA_ADD_LIBRARY_NAME}" $<TARGET_OBJECTS:${objlib}>)
  arqma_set_target_no_relink("${ARQMA_ADD_LIBRARY_NAME}")
  arqma_set_target_strip("${ARQMA_ADD_LIBRARY_NAME}")
  if(ARQMA_ADD_LIBRARY_DEPENDS)
    add_dependencies(${objlib} ${ARQMA_ADD_LIBRARY_DEPENDS})
  endif()
  set_property(TARGET "${ARQMA_ADD_LIBRARY_NAME}" PROPERTY FOLDER "libs")
  target_compile_definitions(${objlib} PRIVATE $<TARGET_PROPERTY:${ARQMA_ADD_LIBRARY_NAME},INTERFACE_COMPILE_DEFINITIONS>)
endfunction()

# Generate header for embedded translations
# Generate header for embedded translations, use target toolchain if depends, otherwise use the
# lrelease and lupdate binaries from the host
# add_subdirectory(translations)
include(ExternalProject)
ExternalProject_Add(generate_translations_header
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/translations"
  BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/translations"
  STAMP_DIR ${LRELEASE_PATH}
  CMAKE_ARGS -DLRELEASE_PATH=${LRELEASE_PATH}
  INSTALL_COMMAND ${CMAKE_COMMAND} -E echo "")
include_directories("${CMAKE_CURRENT_BINARY_DIR}/translations")

if(POLICY CMP0079)
  cmake_policy(SET CMP0079 NEW)
endif()

find_package(PkgConfig REQUIRED)

add_library(libunbound INTERFACE)
add_library(systemd INTERFACE)

pkg_check_modules(UNBOUND libunbound REQUIRED IMPORTED_TARGET)
target_link_libraries(libunbound INTERFACE PkgConfig::UNBOUND)

option(WITH_SYSTEMD "Attempts to link against and enable systemd daemon notification support" ON)
if (WITH_SYSTEMD)
  pkg_check_modules(SYSTEMD libsystemd IMPORTED_TARGET)

  if (SYSTEMD_FOUND)
    target_compile_definitions(systemd INTERFACE ENABLE_SYSTEMD)
    target_link_libraries(systemd INTERFACE PkgConfig::SYSTEMD)
  else()
    warn("systemd not found. building without systemd support (use -DWITH_SYSTEMD=OFF to suppress this warning)")
  endif()
endif()

add_subdirectory(external)

include_directories(${UNBOUND_INCLUDE_DIR})

include_directories(${EASYLOGGING_INCLUDE})
link_directories(${EASYLOGGING_LIBRARY_DIRS})

include_directories(${LMDB_INCLUDE_DIRS})

include_directories(${LIBUNWIND_INCLUDE})
link_directories(${LIBUNWIND_LIBRARY_DIRS})

if(HIDAPI_FOUND)
  statusinfo("Using HIDAPI include dir at ${HIDAPI_INCLUDE_DIR}")
  add_definitions(-DHAVE_HIDAPI)
  include_directories(${HIDAPI_INCLUDE_DIR})
  link_directories(${LIBHIDAPI_LIBRARY_DIRS})
elseif(NOT HIDAPI_FOUND)
  statusinfo("Could not find HIDAPI")
endif()

include(TestCXXAcceptsFlag)
statusinfo("Building on ${CMAKE_SYSTEM_PROCESSOR} for ${ARCH}")
if(ARCH STREQUAL "default")
  set(ARCH_FLAG "")
elseif(PPC64LE)
  set(ARCH_FLAG "-mcpu=power8")
elseif(PPC64)
  set(ARCH_FLAG "-mcpu=970")
elseif(PPC)
  set(ARCH_FLAG "-mcpu=7400")
elseif(IOS AND ARCH STREQUAL "arm64")
  statusinfo("IOS: Changing arch from arm64 to armv8")
  set(ARCH_FLAG "-march=armv8")
elseif(IOS AND ARCH STREQUAL "x86_64")
  statusinfo("IOS: Changing arch from x86_64 to x86-64")
  set(ARCH_FLAG "-march=x86-64")
else()
  set(ARCH_FLAG "-march=${ARCH}")
  if(ARCH STREQUAL "native")
    check_c_compiler_flag(-march=native CC_SUPPORTS_MARCH_NATIVE)
    if (NOT CC_SUPPORTS_MARCH_NATIVE)
      check_c_compiler_flag(-mtune=native CC_SUPPORTS_MTUNE_NATIVE)
      if (CC_SUPPORTS_MTUNE_NATIVE)
        set(ARCH_FLAG "-mtune=${ARCH}")
      else()
        set(ARCH_FLAG "")
      endif()
    endif()
  endif()
endif()

option(NO_AES "Explicitly disable AES support" ${NO_AES})

if(NO_AES)
  statusinfo("AES support explicitly disabled")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNO_AES")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNO_AES")
elseif(NOT ARM AND NOT PPC64LE AND NOT PPC64 AND NOT PPC AND NOT S390X)
  statusinfo("AES support enabled")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -maes")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -maes")
elseif(PPC64LE OR PPC64 OR PPC)
  statusinfo("AES support not available on POWER")
elseif(S390X)
  statusinfo("AES support not available on s390x")
elseif(ARM6)
  statusinfo("AES support not available on ARMv6")
elseif(ARM7)
  statusinfo("AES support not available on ARMv7")
elseif(ARM8)
  CHECK_CXX_ACCEPTS_FLAG("-march=${ARCH}+crypto" ARCH_PLUS_CRYPTO)
  if(ARCH_PLUS_CRYPTO)
    statusinfo("Crypto extensions enabled for ARMv8")
    set(ARCH_FLAG "-march=${ARCH}+crypto")
  else()
    statusinfo("Crypto extensions unavailable on your ARMv8 device")
  endif()
else()
  statusinfo("AES support disabled")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ARCH_FLAG}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ARCH_FLAG}")

set(WARNINGS "-Wall -Wextra -Wpointer-arith -Wundef -Wvla -Wwrite-strings -Wno-error=extra -Wno-error=deprecated-declarations -Wno-unused-parameter -Wno-error=unused-variable -Wno-error=undef -Wno-error=uninitialized -Wno-deprecated-declarations -Wno-unused-variable -Wno-unused-private-field -Wno-unused-but-set-variable")
if(CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  if(ARM)
    set(WARNINGS "${WARNINGS} -Wno-error=inline-asm")
  endif()
else()
  set(WARNINGS "${WARNINGS} -Wlogical-op -Wno-error=maybe-uninitialized -Wno-error=cpp")
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND NOT (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 10.5))
  set(WARNINGS "${WARNINGS} -Wno-string-compare")
endif()
if(MINGW)
  set(WARNINGS "${WARNINGS} -Wno-error=unused-value -Wno-error=unused-but-set-variable")
  set(MINGW_FLAG "${MINGW_FLAG} -DWIN32_LEAN_AND_MEAN")
  set(Boost_THREADAPI win32)
  include_directories(SYSTEM src/platform/mingw)
  # mingw doesn't support LTO (multiple definition errors at link time)
  set(USE_LTO_DEFAULT false)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--stack,10485760")
  if(NOT BUILD_64)
    add_definitions(-DWINVER=0x0600 -D_WIN32_WINNT=0x0600)
  endif()
endif()
if(ARM7)
  set(WARNINGS "${WARNINGS} -Wno-psabi -Wno-maybe-uninitialized")
endif()
set(C_WARNINGS "-Waggregate-return -Wnested-externs -Wold-style-definition -Wstrict-prototypes")
set(CXX_WARNINGS "-Wno-reorder -Wno-missing-field-initializers")

arqma_enable_coverage()
# With GCC 6.1.1 the compiled binary malfunctions due to aliasing. Until that
# is fixed in the code (Issue #847), force compiler to be conservative.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-strict-aliasing")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-strict-aliasing")

# if those don't work for your compiler, single it out where appropriate
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(C_SECURITY_FLAGS "${C_SECURITY_FLAGS} -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=1")
  set(CXX_SECURITY_FLAGS "${CXX_SECURITY_FLAGS} -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=1")
endif()

# warnings
add_c_flag_if_supported(-Wformat C_SECURITY_FLAGS)
add_cxx_flag_if_supported(-Wformat CXX_SECURITY_FLAGS)
add_c_flag_if_supported(-Wformat-security C_SECURITY_FLAGS)
add_cxx_flag_if_supported(-Wformat-security CXX_SECURITY_FLAGS)

#-fstack-protector
if(NOT WIN32 AND NOT IOS)
  add_c_flag_if_supported(-fstack-protector C_SECURITY_FLAGS)
  add_cxx_flag_if_supported(-fstack-protector CXX_SECURITY_FLAGS)
  add_c_flag_if_supported(-fstack-protector-strong C_SECURITY_FLAGS)
  add_cxx_flag_if_supported(-fstack-protector-strong CXX_SECURITY_FLAGS)
endif()

# -fno-stack-check
if (IOS)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-stack-check")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-stack-check")
endif()

# New in GCC 8.2
if(NOT WIN32)
  add_c_flag_if_supported(-fcf-protection=full C_SECURITY_FLAGS)
  add_cxx_flag_if_supported(-fcf-protection=full CXX_SECURITY_FLAGS)
  add_c_flag_if_supported(-fstack-clash-protection C_SECURITY_FLAGS)
  add_cxx_flag_if_supported(-fstack-clash-protection CXX_SECURITY_FLAGS)
endif()

if(NOT (CMAKE_C_COMPILER_ID STREQUAL "GNU" AND NOT CMAKE_C_COMPILER_VERSION VERSION_LESS 9.1))
  add_c_flag_if_supported(-mmitigate-rop C_SECURITY_FLAGS)
  add_cxx_flag_if_supported(-mmitigate-rop CXX_SECURITY_FLAGS)
endif()

# linker
if (NOT SANITIZE AND NOT OSSFUZZ AND NOT (WIN32 AND (CMAKE_C_COMPILER_ID STREQUAL "GNU" AND (CMAKE_C_COMPILER_VERSION VERSION_LESS 9.1 OR NOT STATIC))))
  add_linker_flag_if_supported("-pie" LD_SECURITY_FLAGS)
endif()
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
  add_linker_flag_if_supported(-Wl, -pie LD_SECURITY_FLAGS)
endif()
add_linker_flag_if_supported(-Wl,-z,relro LD_SECURITY_FLAGS)
add_linker_flag_if_supported(-Wl,-z,now LD_SECURITY_FLAGS)
add_linker_flag_if_supported(-Wl,-z,noexecstack noexecstack_SUPPORTED)
if(noexecstack_SUPPORTED)
  set(LD_SECURITY_FLAGS "${LD_SECURITY_FLAGS} -Wl,-z,noexecstack")
endif()
add_linker_flag_if_supported(-Wl,-z,noexecheap noexecheap_SUPPORTED)
if(noexecheap_SUPPORTED)
  set(LD_SECURITY_FLAGS "${LD_SECURITY_FLAGS} -Wl,-z,noexecheap")
endif()

if(BACKCOMPAT)
  add_linker_flag_if_supported(-Wl,--wrap=__divmoddi4 LD_BACKCOMPAT_FLAGS)
  add_linker_flag_if_supported(-Wl,--wrap=glob LD_BACKCOMPAT_FLAGS)
  statusinfo("Using Lib C back compat flags: ${LD_BACKCOMPAT_FLAGS}")
endif()

# some windows linker bits
if(WIN32)
  add_linker_flag_if_supported(-Wl,--dynamicbase LD_SECURITY_FLAGS)
  add_linker_flag_if_supported(-Wl,--nxcompat LD_SECURITY_FLAGS)
  add_linker_flag_if_supported(-Wl,--high-entropy-va LD_SECURITY_FLAGS)
endif()

add_c_flag_if_supported(-Werror=switch C_SECURITY_FLAGS)
add_cxx_flag_if_supported(-Werror=swith CXX_SECURITY_FLAGS)
add_c_flag_if_supported(-Werror=return-type C_SECURITY_FLAGS)
add_cxx_flag_if_supported(-Werror=return-type CXX_SECURITY_FLAGS)

statusinfo("Using C security hardening flags: ${C_SECURITY_FLAGS}")
statusinfo("Using C++ security hardening flags: ${CXX_SECURITY_FLAGS}")
statusinfo("Using linker security hardening flags: ${LD_SECURITY_FLAGS}")

if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
  set(LINKER_OPTIONS "${LINKER_OPTIONS} -ffunction-sections -fdata-sections -Wl,--gc-sections")
endif()
if(CMAKE_C_COMPILER_ID MATCHES "Clang" AND APPLE)
  set(LINKER_OPTIONS "${LINKER_OPTIONS} -ffunction-sections -fdata-sections -Wl,-dead_strip")
  set(WARNINGS "${WARNINGS} -Wno-unused-command-line-argument")
endif()
if(DEPENDS OR MINGW AND NOT FREEBSD AND NOT APPLE)
  set(CMAKE_EXE_LINKER_OPTIONS_FLAGS "${CMAKE_EXE_LINKER_OPTIONS_FLAGS} -Wl,-s")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GNU_SOURCE ${MINGW_FLAG} ${WARNINGS} ${C_WARNINGS} ${PIC_FLAG} ${C_SECURITY_FLAGS} ${LINKER_OPTIONS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GNU_SOURCE ${MINGW_FLAG} ${WARNINGS} ${CXX_WARNINGS} ${PIC_FLAG} ${CXX_SECURITY_FLAGS} ${LINKER_OPTIONS} ${CMAKE_EXE_LINKER_OPTIONS_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${LD_SECURITY_FLAGS} ${LD_BACKCOMPAT_FLAGS}")

# With GCC 6.1.1 the compiled binary malfunctions due to aliasing. Until that
# is fixed in the code (Issue #847), force compiler to be conservative.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-strict-aliasing")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-strict-aliasing")

if(ARM)
  statusinfo("Setting FPU Flags for ARM Processors")

    #NB NEON hardware does not fully implement the IEEE 754 standard for floating-point arithmetic
    #Need custom assembly code to take full advantage of NEON SIMD

    #Cortex-A5/9  -mfpu=neon-fp16
    #Cortex-A7/15 -mfpu=neon-vfpv4
    #Cortex-A8    -mfpu=neon
    #ARMv8        -FP and SIMD on by default for all ARM8v-A series, NO -mfpu setting needed

    #For custom -mtune, processor IDs for ARMv8-A series:
    #0xd04 - Cortex-A35
    #0xd07 - Cortex-A57
    #0xd08 - Cortex-A72
    #0xd03 - Cortex-A73

  if(NOT ARM8)
    CHECK_CXX_ACCEPTS_FLAG(-mfpu=vfp3-d16 CXX_ACCEPTS_VFP3_D16)
    CHECK_CXX_ACCEPTS_FLAG(-mfpu=vfp4 CXX_ACCEPTS_VFP4)
    CHECK_CXX_ACCEPTS_FLAG(-mfloat-abi=hard CXX_ACCEPTS_MFLOAT_HARD)
    CHECK_CXX_ACCEPTS_FLAG(-mfloat-abi=softfp CXX_ACCEPTS_MFLOAT_SOFTFP)
  endif()

    if(ARM8)
      CHECK_CXX_ACCEPTS_FLAG(-mfix-cortex-a53-835769 CXX_ACCEPTS_MFIX_CORTEX_A53_835769)
      CHECK_CXX_ACCEPTS_FLAG(-mfix-cortex-a53-843419 CXX_ACCEPTS_MFIX_CORTEX_A53_843419)
    endif()

    if(ARM6)
      statusinfo("Selecting VFP for ARMv6")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfpu=vfp")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfpu=vfp")
      if(DEPENDS)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -marm")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -marm")
      endif()
    endif(ARM6)

    if(ARM7)
      if(CXX_ACCEPTS_VFP3_D16 AND NOT CXX_ACCEPTS_VFP4)
        statusinfo("Selecting VFP3 for ARMv7")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfpu=vfp3-d16")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfpu=vfp3-d16")
      endif()

      if(CXX_ACCEPTS_VFP4)
        statusinfo("Selecting VFP4 for ARMv7")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfpu=vfp4")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfpu=vfp4")
      endif()

      if(CXX_ACCEPTS_MFLOAT_HARD)
        statusinfo("Setting Hardware ABI for Floating Point")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfloat-abi=hard")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfloat-abi=hard")
      endif()

      if(CXX_ACCEPTS_MFLOAT_SOFTFP AND NOT CXX_ACCEPTS_MFLOAT_HARD)
        statusinfo("Setting Software ABI for Floating Point")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfloat-abi=softfp")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfloat-abi=softfp")
      endif()
    endif(ARM7)

    if(ARM8)
      if(CXX_ACCEPTS_MFIX_CORTEX_A53_835769)
        statusinfo("Enabling Cortex-A53 workaround 835769")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfix-cortex-a53-835769")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfix-cortex-a53-835769")
      endif()

      if(CXX_ACCEPTS_MFIX_CORTEX_A53_843419)
        statusinfo("Enabling Cortex-A53 workaround 843419")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfix-cortex-a53-843419")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfix-cortex-a53-843419")
      endif()
    endif(ARM8)

  endif(ARM)

  if(NOT SANITIZE AND ANDROID AND NOT BUILD_GUI_DEPS STREQUAL "ON" OR IOS)
    # From Android 5: "only position independent executables (PIE) are supported"
    statusinfo("Enabling PIE executable")
    set(PIC_FLAG "")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIE")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_CXX_FLAGS} -fPIE -pie")
  endif()

if(APPLE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=default -DGTEST_HAS_TR1_TUPLE=0")
  if(ARM)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-aligned-allocation")
  endif()
endif()

  set(DEBUG_FLAGS "-g3")

  # At least some CLANGs default to not enough for Arqma
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftemplate-depth=900")

  if(NOT DEFINED USE_LTO_DEFAULT)
    set(USE_LTO_DEFAULT false)
  endif()
  set(USE_LTO ${USE_LTO_DEFAULT} CACHE BOOL "Use Link-Time Optimization (Release mode only)")

  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # There is a clang bug that does not allow to compile code that uses AES-NI intrinsics if -flto is enabled, so explicitly disable
    set(USE_LTO false)
  endif()

  if(USE_LTO)
    set(RELEASE_FLAGS "${RELEASE_FLAGS} -flto")
    if(STATIC)
      set(RELEASE_FLAGS "${RELEASE_FLAGS} -ffat-lto-objects")
    endif()
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.9.0))
      if(DEFINED CHOST)
        set(CMAKE_AR "${CHOST}-gcc-ar")
        set(CMAKE_RANLIB "${CHOST}-gcc-ranlib")
      else()
        set(CMAKE_AR "gcc-ar")
        set(CMAKE_RANLIB "gcc-ranlib")
      endif()
    endif()
  endif()

  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${DEBUG_FLAGS}")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${DEBUG_FLAGS}")
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${RELEASE_FLAGS}")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${RELEASE_FLAGS}")

  if(STATIC)
    if(MINGW)
      set(STATIC_FLAGS "-static")
    elseif(NOT APPLE)
      set(STATIC_FLAGS "-static-libgcc -static-libstdc++")
    endif()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${STATIC_FLAGS} ")
  endif()

cmake_policy(SET CMP0093 NEW)

set(Boost_USE_MULTITHREADED ON)

if(${BOOST_IGNORE_SYSTEM_PATHS} STREQUAL "ON")
  set(Boost_NO_SYSTEM_PATHS TRUE)
endif()

set(OLD_LIB_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
if(APPLE AND NOT DEPENDS)
  set(Boost_NO_BOOST_CMAKE ON)
endif()

if(STATIC)
  if(MINGW)
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
  endif()

  if(NOT APPLE)
    set(Boost_USE_STATIC_LIBS ON)
    set(Boost_USE_STATIC_RUNTIME ON)
  endif()
endif()

if(DEPENDS)
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(Boost_ARCHITECTURE "-x64")
  else()
    set(Boost_ARCHITECTURE "-x32")
  endif()
endif()

set(Boost_version_min 1.66)
find_package(Boost ${Boost_version_min} QUIET REQUIRED)

if(NOT Boost_FOUND)
  die("Could not find Boost libraries, please make sure you have installed Boost or libboost-all-dev. Minimum version 1.66 is required")
elseif(Boost_FOUND)
  statusinfo("Found Boost Version: ${Boost_VERSION_STRING}")

  set(Boost_Libs date_time chrono serialization program_options thread filesystem)
  if(WIN32)
    list(APPEND Boost_Libs locale)
  endif()

  if(Boost_VERSION_STRING VERSION_LESS 1.69.0)
    list(APPEND Boost_Libs system)
  endif()

  if(Boost_VERSION_STRING VERSION_LESS 1.77.0)
    list(APPEND Boost_Libs regex)
  endif()

  statusinfo("Boost Libraries: ${Boost_Libs}")

  find_package(Boost ${Boost_version_min} QUIET REQUIRED COMPONENTS ${Boost_Libs})
  set(CMAKE_FIND_LIBRARY_SUFFIXES ${OLD_LIB_SUFFIXES})
endif()

add_definitions(-DBOOST_ASIO_ENABLE_SEQUENTIAL_STRAND_ALLOCATION)
add_definitions(-DBOOST_NO_AUTO_PTR)
add_definitions(-DBOOST_UUID_DISABLE_ALIGNMENT)

include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
if(MINGW)
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wa,-mbig-obj")
  set(EXTRA_LIBRARIES mswsock ws2_32 iphlpapi crypt32 bcrypt ssp)
  if(NOT DEPENDS)
    set(ICU_LIBRARIES icuio icuin icuuc icudt icutu iconv)
  endif()
elseif(APPLE OR OPENBSD OR ANDROID)
  set(EXTRA_LIBRARIES "")
elseif(FREEBSD)
  set(EXTRA_LIBRARIES execinfo)
elseif(DRAGONFLY)
  find_library(COMPAT compat)
  set(EXTRA_LIBRARIES execinfo ${COMPAT})
elseif(CMAKE_SYSTEM_NAME MATCHES "(SunOS|Solaris)")
  set(EXTRA_LIBRARIES socket nsl resolv)
elseif(NOT DEPENDS)
  find_library(RT rt)
  find_library(Z z)
  set(EXTRA_LIBRARIES ${RT} ${Z})
endif()

list(APPEND EXTRA_LIBRARIES ${CMAKE_DL_LIBS})

if(HIDAPI_FOUND OR LibUSB_COMPILE_TEST_PASSED)
  if(APPLE)
    if(DEPENDS)
      list(APPEND EXTRA_LIBRARIES "-framework Foundation -framework AppKit -framework IOKit")
    else()
      find_library(COREFOUNDATION CoreFoundation)
      find_library(APPKIT AppKit)
      find_library(IOKIT IOKit)
      list(APPEND EXTRA_LIBRARIES ${APPKIT})
      list(APPEND EXTRA_LIBRARIES ${IOKIT})
      list(APPEND EXTRA_LIBRARIES ${COREFOUNDATION})
    endif()
  endif()
  if(WIN32)
    list(APPEND EXTRA_LIBRARIES setupapi)
  endif()
endif()

option(USE_READLINE "Build with GNU readline support." ON)
if(USE_READLINE AND NOT DEPENDS)
  find_package(Readline)
  if(READLINE_FOUND AND GNU_READLINE_FOUND)
    add_library(readline INTERFACE)
    if(STATIC)
      find_path(Readline_INCLUDE_PATH readline/readline.h)
      find_library(Readline_LIBRARY readline)
      find_library(Termcap_LIBRARY NAMES tinfo termcap ncursesw ncurses cursesw curses)
      target_link_libraries(readline INTERFACE ${Readline_LIBRARY} ${Termcap_LIBRARY})
    endif()
    target_link_libraries(readline INTERFACE ${GNU_READLINE_LIBRARY})
    target_include_directories(readline INTERFACE ${Readline_INCLUDE_DIR})
    target_compile_definitions(readline INTERFACE HAVE_READLINE)
    statusinfo("Found readline library at: ${GNU_READLINE_LIBRARY}")
  else()
    statusinfo("Could not find GNU readline library so building without readline support")
  endif()
elseif(USE_READLINE AND DEPENDS AND NOT MINGW)
  find_path(Readline_INCLUDE_PATH readline/readline.h)
  find_library(Readline_LIBRARY readline)
  find_library(Terminfo_LIBRARY tinfo)
  add_library(readline INTERFACE)
  target_link_libraries(readline INTERFACE ${Readline_LIBRARY} ${Terminfo_LIBRARY})
  target_include_directories(readline INTERFACE ${Readline_INCLUDE_PATH})
  target_compile_definitions(readline INTERFACE HAVE_READLINE)
endif()

if(ANDROID)
  set(ATOMIC libatomic.a)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=user-defined-warnings")
endif()
if(CMAKE_C_COMPILER_ID MATCHES "Clang" AND ARCH_WIDTH EQUAL "32" AND NOT IOS AND NOT FREEBSD)
  find_library(ATOMIC atomic)
  if(ATOMIC_FOUND)
    list(APPEND EXTRA_LIBRARIES ${ATOMIC})
  endif()
endif()

option(ARQMA_DEBUG_SHORT_PROOFS "Quick proofs on test networks" OFF)
if (ARQMA_DEBUG_SHORT_PROOFS)
  add_definitions(-DUPTIME_PROOF_BASE_MINUTE=3)
endif()

if(STATIC)
  set(sodium_USE_STATIC_LIBS ON)
endif()

pkg_check_modules(SODIUM libsodium>=1.0.18 REQUIRED)
pkg_check_modules(ZMQ libzmq>=4.3.2 REQUIRED)

if(SODIUM_FOUND)
  statusinfo("Found libsodium at ${SODIUM_LIBRARIES} with headers at ${SODIUM_INCLUDE_DIRS}")
  add_library(sodium INTERFACE)
  find_library(sodium_link_libs NAMES ${SODIUM_LIBRARIES} PATHS ${SODIUM_LIBRARY_DIRS})
  target_link_libraries(sodium INTERFACE ${sodium_link_libs})
  target_include_directories(sodium INTERFACE ${SODIUM_INCLUDE_DIRS})
else()
  die("libsodium not found while is REQUIRED!!! Install libsodium and re-configure them")
endif()

if(NOT ZMQ_FOUND)
  die("libzmq not found")
elseif(ZMQ_FOUND)
  statusinfo("Found libzmq at ${ZMQ_LIBRARIES} with headers at ${ZMQ_INCLUDE_DIRS}")
  add_library(libzmq INTERFACE)
  find_library(libzmq_link_libs NAMES ${ZMQ_LIBRARIES} PATHS ${ZMQ_LIBRARY_DIRS})
  target_link_libraries(libzmq INTERFACE ${libzmq_link_libs} sodium)
  target_link_directories(libzmq INTERFACE ${ZMQ_INCLUDE_DIRS} ${SODIUM_INCLUDE_DIRS})
  if(MINGW OR WIN32)
    target_link_libraries(libzmq INTERFACE ws2_32 iphlpapi crypt32)
  endif()
endif()
include_directories(${ZMQ_INCLUDE_DIRS})

add_subdirectory(contrib)
add_subdirectory(src)

if(BUILD_TESTS)
  add_subdirectory(tests)
endif()

if(BUILD_DEBUG_UTILITIES)
  statusinfo("Building debug utilities")
else()
  statusinfo("Not Building debug utilities")
endif()

if(BUILD_DOCUMENTATION)
  set(DOC_GRAPHS "YES" CACHE STRING "Create dependency graphs (needs graphviz)")
  set(DOC_FULLGRAPHS "NO" CACHE STRING "Create call/callee graphs (large)")

  find_program(DOT_PATH dot)

  if(DOT_PATH STREQUAL "DOT_PATH-NOTFOUND")
    statusinfo("Doxygen: graphviz not found - graphs disabled")
    set(DOC_GRAPHS "NO")
  endif()

  find_package(Doxygen)
  if(DOXYGEN_FOUND)
    configure_file("cmake/Doxyfile.in" "Doxyfile" @ONLY)
    configure_file("cmake/Doxygen.extra.css.in" "Doxygen.extra.css" @ONLY)
    add_custom_target(doc
      ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMENT "Generating API documentation with Doxygen.." VERBATIM)
  endif()
endif()

# when ON - will install libwallet_merged into "lib"
option(BUILD_GUI_DEPS "Build GUI dependencies." OFF)
