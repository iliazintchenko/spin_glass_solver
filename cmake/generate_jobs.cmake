#-------------------------------------------------------
# Slurm script generator for benchmarking program
#
# We would like to generate a script which can be used to generate batch jobs
# to submit benchmarks to slurm using many combinations of settings.
#
# We add a custom command which takes our template script and
# expands out all the variables we need to pass into it.
# Unfortunately, due to the way cmake works, some variables are only known
# at build time, and not at cmake configure time. Using a custom command which
# calls cmake to run our script at build time, allows us to pass variables
# into the final script which is placed in our build dir.
#
# Note that we generate these scripts in the build dir instead
# of the install dir as they are intended for development testing.
# A version could be supported for installation later.
#-------------------------------------------------------

# not required but can be useful sometimes for checking problems
  #set(LD_LIBRARY_PATH "${HPX_LIBRARY_OUTPUT_DIRECTORY_${CMAKE_CFG_INTDIR}}" CACHE PATH "Path to set when generating script tests")

# Make sure scripts dir exists
if (NOT ${PROJECT_NAME}_BENCHMARK_SCRIPTS_PATH)
  set(${PROJECT_NAME}_BENCHMARK_SCRIPTS_PATH ${PROJECT_BINARY_DIR}/scripts)
endif()
message("${PROJECT_NAME}_BENCHMARK_SCRIPTS_PATH is ${${PROJECT_NAME}_BENCHMARK_SCRIPTS_PATH}")

execute_process(COMMAND "${CMAKE_COMMAND}" -E make_directory "${${PROJECT_NAME}_BENCHMARK_SCRIPTS_PATH}")

#--------------------------------------------------
# Slurm script generator for running many tests
#--------------------------------------------------
set(SCRIPTS "slurm-benchmark")
foreach(script ${SCRIPTS})
  ADD_CUSTOM_COMMAND(
      DEPENDS   "${CMAKE_CURRENT_LIST_DIR}/${script}.sh.in"
      COMMAND   "${CMAKE_COMMAND}"
      ARGS      -DEXE_PATH="$<TARGET_FILE:spinsolve>"
		-DMPIEXEC="${MPIEXEC}"
		-DSCRIPT_SOURCE_DIR="${CMAKE_CURRENT_LIST_DIR}"
		-DSCRIPT_NAME=${script}
		-DSCRIPT_DEST_DIR="${${PROJECT_NAME}_BENCHMARK_SCRIPTS_PATH}"
		-DLIB_PATH="${LD_LIBRARY_PATH}"
		-DJOB_OPTIONS1="${SLURM_JOB_OPTIONS1}"
		-P "${CMAKE_CURRENT_LIST_DIR}/copy_script.cmake"
      OUTPUT    "${${PROJECT_NAME}_BENCHMARK_SCRIPTS_PATH}/${script}.sh"
      VERBATIM
  )

  add_custom_target(script-${script}
      DEPENDS "${${PROJECT_NAME}_BENCHMARK_SCRIPTS_PATH}/${script}.sh"
  )
endforeach(script)

