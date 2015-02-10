cmake_minimum_required(VERSION 2.8)
project(spin_glass_solver)

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
# Flags
#--------------------------------------------------
# needed for all compilers
set(CMAKE_CXX_FLAGS "-std=c++11")

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  set(CMAKE_BUILD_TYPE RELEASE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -pedantic ")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  set(CMAKE_BUILD_TYPE RELEASE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -pedantic ")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
endif()

#--------------------------------------------------
# Find HPX
#--------------------------------------------------
find_package(HPX REQUIRED)
set(CMAKE_CXX_FLAGS ${HPX_CXX_FLAGS})
include_directories(${HPX_INCLUDE_DIRS})

#--------------------------------------------------
# Find Boost (should already be found by HPX anyway)
#--------------------------------------------------
find_package(Boost 
    COMPONENTS 
        program_options thread system date_time chrono serialization atomic filesystem 
    REQUIRED
)
if (WIN32)
  add_definitions(-DBOOST_PROGRAM_OPTIONS_DYN_LINK)
endif()

#--------------------------------------------------
# Library
#--------------------------------------------------
add_library(solver STATIC 
  src/result.cpp 
  src/hamiltonian.cpp 
  src/sa_solver.cpp
)

#--------------------------------------------------
# Exe
#--------------------------------------------------
add_executable(spinsolve src/main.cpp)
message("Bloody boost lib ${Boost_LIBRARIES} is ${Boost_PROGRAM_OPTIONS_LIBRARY}")
target_link_libraries(spinsolve 
    solver 
    hpx 
    hpx_init 
    hpx_serialization 
    iostreams_component 
    ${HPX_LIBRARIES} 
    ${Boost_LIBRARIES}
)

#--------------------------------------------------
# Install
#--------------------------------------------------
install(TARGETS spinsolve solver
        RUNTIME DESTINATION bin
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        )
SET(CMAKE_INSTALL_PREFIX ${CMAKE_HOME_DIRECTORY})

