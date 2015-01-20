cmake_minimum_required(VERSION 2.8)
project(spin_glass_solver)
set(CMAKE_CXX_FLAGS "-std=c++11 -O3 -Wall -pedantic")
set(CMAKE_BUILD_TYPE RELEASE)
add_library(solver STATIC src/result.cpp src/hamiltonian.cpp src/sa_solver.cpp)
add_executable(main src/main.cpp)
target_link_libraries(main solver)
install(TARGETS main solver
        RUNTIME DESTINATION bin
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        )
SET(CMAKE_INSTALL_PREFIX ${CMAKE_HOME_DIRECTORY})

