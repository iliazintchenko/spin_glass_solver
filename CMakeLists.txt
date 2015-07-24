cmake_minimum_required(VERSION 2.8)
project(spin_glass_solver)

# Setup cmake policies.
foreach(p
    CMP0012
    CMP0013
    CMP0014
    CMP0022 # CMake 2.8.12
    CMP0025 # CMake 3.0
    CMP0043 # CMake 3.0
    CMP0053 # CMake 3.1
  )
  if(POLICY ${p})
    cmake_policy(SET ${p} NEW)
  endif()
endforeach()

#--------------------------------------------------
# Setup Output directories
#--------------------------------------------------
SET (CMAKE_RUNTIME_OUTPUT_DIRECTORY
    ${PROJECT_BINARY_DIR}/bin CACHE PATH "Single Directory for all Executables."
)
SET (CMAKE_LIBRARY_OUTPUT_DIRECTORY
    ${PROJECT_BINARY_DIR}/bin CACHE PATH "Single Directory for all Libraries"
)
SET (CMAKE_ARCHIVE_OUTPUT_DIRECTORY
    ${PROJECT_BINARY_DIR}/bin CACHE PATH "Single Directory for all static libraries."
)

#--------------------------------------------------
# CMake include path
#--------------------------------------------------
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

#--------------------------------------------------
# Flags
#--------------------------------------------------
# needed for all compilers
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
#  set(CMAKE_BUILD_TYPE RELEASE)
#  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -pedantic ")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
#  set(CMAKE_BUILD_TYPE RELEASE)
#  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -pedantic ")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
endif()

#--------------------------------------------------
# Find Boost (should already be found by HPX anyway)
#--------------------------------------------------
find_package(Boost 1.56
    COMPONENTS 
        program_options thread system date_time chrono serialization atomic filesystem 
    REQUIRED
)
if (WIN32)
  #add_definitions(-DBOOST_PROGRAM_OPTIONS_DYN_LINK)
  add_definitions(-DBOOST_ALL_DYN_LINK)
endif()

#--------------------------------------------------
# Find HPX
#--------------------------------------------------
find_package(HPX REQUIRED)
#set(CMAKE_CXX_FLAGS ${HPX_CXX_FLAGS})
include_directories(${HPX_INCLUDE_DIRS})

#--------------------------------------------------
# Use our subset of KWSys
#--------------------------------------------------
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/kwsys)

#--------------------------------------------------
# Our multi-thread wrappers 
#--------------------------------------------------
include_directories(src)
include_directories(async)

#--------------------------------------------------
# compile definitions 
#--------------------------------------------------
# use these for default input filenames and output locations
add_definitions(-DSPINSOLVE_SOURCE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}\")
add_definitions(-DSPINSOLVE_BINARY_DIR=\"${CMAKE_CURRENT_BINARY_DIR}\")
add_definitions(-DKWSYS_NAMESPACE=kwsys)

#--------------------------------------------------
# Library
#--------------------------------------------------
add_library(solver STATIC 
  src/result.cpp 
  src/hamiltonian.cpp 
  src/sa_solver.cpp
  src/CommandCapture.cpp
  src/kwsys/ProcessUNIX.c
  src/kwsys/System.c
)
if (WIN32)
 # add src/kwsys/ProcessUNIX.c
endif()

#--------------------------------------------------
# Exe
#--------------------------------------------------
add_executable(spinsolve src/main.cpp)
hpx_setup_target(spinsolve)
target_link_libraries(spinsolve 
    solver 
    iostreams_component 
    ${Boost_LIBRARIES}
)
target_compile_definitions(spinsolve PUBLIC -DAPP_BINARY_NAME=\"$<TARGET_FILE:spinsolve>\")

#--------------------------------------------------
# Exe, reference code that runs on single local thread
#--------------------------------------------------
add_definitions(-DHPX_APPLICATION_EXPORTS)
add_executable(solver_reference src/single_thread.cpp)
hpx_setup_target(solver_reference)
target_link_libraries(solver_reference 
    solver
    iostreams_component 
    ${Boost_LIBRARIES}
)

#--------------------------------------------------
# Generate benchmarking scripts
#--------------------------------------------------
hpx_option(${PROJECT_NAME}_BENCHMARK_SCRIPTS_PATH PATH
  "Directory to place batch scripts in"
  "${spin_glass_solver_BINARY_DIR}/scripts"
  ADVANCED)

include(generate_jobs)

#--------------------------------------------------
# Install
#--------------------------------------------------
install(TARGETS spinsolve solver
        RUNTIME DESTINATION bin
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        )
SET(CMAKE_INSTALL_PREFIX ${CMAKE_HOME_DIRECTORY})

