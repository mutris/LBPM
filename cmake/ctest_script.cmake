# ctest script for building, running, and submitting the test results 
# Usage:  ctest -s script,build
#   build = debug / optimized / weekly / valgrind / valgrind-matlab
# Note: this test will use use the number of processors defined in the variable N_PROCS,
#   the enviornmental variable N_PROCS, or the number of processors availible (if not specified)

# Set platform specific variables
SITE_NAME( HOSTNAME )
SET( CC                 $ENV{CC}                )
SET( CXX                $ENV{CXX}               )
SET( MPIEXEC            $ENV{MPIEXEC}           )
SET( USE_TIMER          "$ENV{USE_TIMER}"       )
SET( TIMER_DIRECTORY    "$ENV{TIMER_DIRECTORY}" )
SET( RATES_DIRECTORY    "$ENV{RATES_DIRECTORY}" )
SET( USE_ACML           $ENV{USE_ACML}          )
SET( ACML_DIRECTORY     $ENV{ACML_DIRECTORY}    )
SET( USE_MKL            $ENV{USE_MKL}           )
SET( MKL_DIRECTORY      $ENV{MKL_DIRECTORY}     )
SET( BLAS_DIRECTORY     $ENV{BLAS_DIRECTORY}    )
SET( BLAS_LIB           $ENV{BLAS_LIB}          )
SET( LAPACK_DIRECTORY   $ENV{LAPACK_DIRECTORY}  )
SET( LAPACK_LIB         $ENV{LAPACK_LIB}        )
SET( USE_MATLAB         $ENV{USE_MATLAB}        )
SET( MATLAB_DIRECTORY   $ENV{MATLAB_DIRECTORY}  )
SET( COVERAGE_COMMAND   $ENV{COVERAGE_COMMAND}  )
SET( VALGRIND_COMMAND   $ENV{VALGRIND_COMMAND}  )
SET( CMAKE_MAKE_PROGRAM $ENV{CMAKE_MAKE_PROGRAM} )
SET( CTEST_CMAKE_GENERATOR $ENV{CTEST_CMAKE_GENERATOR} )
SET( LDLIBS             $ENV{LDLIBS}            )
SET( LDFLAGS            $ENV{LDFLAGS}           )
SET( MPI_COMPILER       $ENV{MPI_COMPILER}      )
SET( MPI_DIRECTORY      $ENV{MPI_DIRECTORY}     )
SET( MPI_INCLUDE        $ENV{MPI_INCLUDE}       )
SET( MPI_LINK_FLAGS     $ENV{MPI_LINK_FLAGS}    )
SET( MPI_LIBRARIES      $ENV{MPI_LIBRARIES}     )
SET( MPIEXEC            $ENV{MPIEXEC}           )
SET( BUILD_SERIAL       $ENV{BUILD_SERIAL}      )
SET( CUDA_FLAGS         $ENV{CUDA_FLAGS}        )
SET( CUDA_HOST_COMPILER $ENV{CUDA_HOST_COMPILER} )


# Get the source directory based on the current directory
IF ( NOT LBPM_SOURCE_DIR )
    SET( LBPM_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." )
ENDIF()
IF ( NOT CMAKE_MAKE_PROGRAM )
    SET( CMAKE_MAKE_PROGRAM make )
ENDIF()


# Check that we specified the build type to run
SET( USE_VALGRIND FALSE )
SET( RUN_WEEKLY FALSE )
SET( USE_CUDA FALSE )
SET( ENABLE_GCOV "false" )
SET( CTEST_COVERAGE_COMMAND ${COVERAGE_COMMAND} )
IF( NOT CTEST_SCRIPT_ARG )
    MESSAGE(FATAL_ERROR "No build specified: ctest -S /path/to/script,build (debug/optimized/valgrind")
ELSEIF( ${CTEST_SCRIPT_ARG} STREQUAL "debug" )
    SET( CTEST_BUILD_NAME "LBPM-WIA-debug" )
    SET( CMAKE_BUILD_TYPE "Debug" )
    SET( ENABLE_GCOV "true" )
ELSEIF( ${CTEST_SCRIPT_ARG} STREQUAL "debug-cuda" )
    SET( CTEST_BUILD_NAME "LBPM-WIA-debug-cuda" )
    SET( CMAKE_BUILD_TYPE "Debug" )
    SET( ENABLE_GCOV "true" )
    SET( USE_CUDA TRUE )
ELSEIF( (${CTEST_SCRIPT_ARG} STREQUAL "optimized") OR (${CTEST_SCRIPT_ARG} STREQUAL "opt") )
    SET( CTEST_BUILD_NAME "LBPM-WIA-opt" )
    SET( CMAKE_BUILD_TYPE "Release" )
ELSEIF( (${CTEST_SCRIPT_ARG} STREQUAL "optimized-cuda") OR (${CTEST_SCRIPT_ARG} STREQUAL "opt-cuda") )
    SET( CTEST_BUILD_NAME "LBPM-WIA-opt-cuda" )
    SET( CMAKE_BUILD_TYPE "Release" )
    SET( USE_CUDA TRUE )
ELSEIF( (${CTEST_SCRIPT_ARG} STREQUAL "weekly") )
    SET( CTEST_BUILD_NAME "LBPM-WIA-weekly" )
    SET( CMAKE_BUILD_TYPE "Release" )
    SET( RUN_WEEKLY TRUE )
ELSEIF( (${CTEST_SCRIPT_ARG} STREQUAL "weekly-cuda") )
    SET( CTEST_BUILD_NAME "LBPM-WIA-weekly-cuda" )
    SET( CMAKE_BUILD_TYPE "Release" )
    SET( RUN_WEEKLY TRUE )
    SET( USE_CUDA TRUE )
ELSEIF( ${CTEST_SCRIPT_ARG} STREQUAL "valgrind" )
    SET( CTEST_BUILD_NAME "LBPM-WIA-valgrind" )
    SET( CMAKE_BUILD_TYPE "Debug" )
    SET( USE_VALGRIND TRUE )
ELSEIF( ${CTEST_SCRIPT_ARG} STREQUAL "valgrind-cuda" )
    SET( CTEST_BUILD_NAME "LBPM-WIA-valgrind-cuda" )
    SET( CMAKE_BUILD_TYPE "Debug" )
    SET( USE_VALGRIND TRUE )
    SET( USE_CUDA TRUE )
ELSE()
    MESSAGE(FATAL_ERROR "Invalid build (${CTEST_SCRIPT_ARG}): ctest -S /path/to/script,build (debug/opt/valgrind")
ENDIF()
IF ( NOT CTEST_COVERAGE_COMMAND )
    SET( ENABLE_GCOV "false" )
ENDIF()


# Set the number of processors
IF( NOT DEFINED N_PROCS )
    SET( N_PROCS $ENV{N_PROCS} )
ENDIF()
IF( NOT DEFINED N_PROCS )
    SET(N_PROCS 1)
    # Linux:
    SET(cpuinfo_file "/proc/cpuinfo")
    IF(EXISTS "${cpuinfo_file}")
        FILE(STRINGS "${cpuinfo_file}" procs REGEX "^processor.: [0-9]+$")
        list(LENGTH procs N_PROCS)
    ENDIF()
    # Mac:
    IF(APPLE)
        find_program(cmd_sys_pro "sysctl")
        if(cmd_sys_pro)
            execute_process(COMMAND ${cmd_sys_pro} hw.physicalcpu OUTPUT_VARIABLE info)
            STRING(REGEX REPLACE "^.*hw.physicalcpu: ([0-9]+).*$" "\\1" N_PROCS "${info}")
        ENDIF()
    ENDIF()
    # Windows:
    IF(WIN32)
        SET(N_PROCS "$ENV{NUMBER_OF_PROCESSORS}")
    ENDIF()
ENDIF()


# Set basic variables
SET( CTEST_PROJECT_NAME "LBPM-WIA" )
SET( CTEST_SOURCE_DIRECTORY "${LBPM_SOURCE_DIR}" )
SET( CTEST_BINARY_DIRECTORY "." )
SET( CTEST_DASHBOARD "Nightly" )
SET( CTEST_CUSTOM_MAXIMUM_NUMBER_OF_ERRORS 500 )
SET( CTEST_CUSTOM_MAXIMUM_NUMBER_OF_WARNINGS 500 )
SET( CTEST_CUSTOM_MAXIMUM_PASSED_TEST_OUTPUT_SIZE 10000 )
SET( CTEST_CUSTOM_MAXIMUM_FAILED_TEST_OUTPUT_SIZE 10000 )
SET( NIGHTLY_START_TIME "18:00:00 EST" )
SET( CTEST_NIGHTLY_START_TIME "22:00:00 EST" )
SET( CTEST_COMMAND "\"${CTEST_EXECUTABLE_NAME}\" -D ${CTEST_DASHBOARD}" )
IF ( BUILD_SERIAL )
    SET( CTEST_BUILD_COMMAND "${CMAKE_MAKE_PROGRAM} -i install" )
ELSE()
    SET( CTEST_BUILD_COMMAND "${CMAKE_MAKE_PROGRAM} -i -j ${N_PROCS} install" )
ENDIF()
SET( CTEST_CUSTOM_WARNING_EXCEPTION "has no symbols" )


# Set timeouts: 30 minutes for debug, 15 for opt, and 60 minutes for valgrind/weekly
IF ( USE_VALGRIND )
    SET( CTEST_TEST_TIMEOUT 3600 )
ELSEIF ( RUN_WEEKLY )
    SET( CTEST_TEST_TIMEOUT 3600 )
ELSEIF( ${CMAKE_BUILD_TYPE} STREQUAL "Debug" )
    SET( CTEST_TEST_TIMEOUT 1800 )
ELSE()
    SET( CTEST_TEST_TIMEOUT 900 )
ENDIF()


# Set valgrind options
#SET (VALGRIND_COMMAND_OPTIONS "--tool=memcheck --leak-check=yes --track-fds=yes --num-callers=50 --show-reachable=yes --trace-children=yes --track-origins=yes --malloc-fill=0xff --free-fill=0xfe --suppressions=${LBPM_SOURCE_DIR}/ValgrindSuppresionFile" )
SET( VALGRIND_COMMAND_OPTIONS  "--tool=memcheck --leak-check=yes --track-fds=yes --num-callers=50 --show-reachable=yes --trace-children=yes --suppressions=${LBPM_SOURCE_DIR}/ValgrindSuppresionFile" )
IF ( USE_VALGRIND )
    SET( MEMORYCHECK_COMMAND ${VALGRIND_COMMAND} )
    SET( MEMORYCHECKCOMMAND ${VALGRIND_COMMAND} )
    SET( CTEST_MEMORYCHECK_COMMAND ${VALGRIND_COMMAND} )
    SET( CTEST_MEMORYCHECKCOMMAND ${VALGRIND_COMMAND} )
    SET( CTEST_MEMORYCHECK_COMMAND_OPTIONS ${VALGRIND_COMMAND_OPTIONS} )
    SET( CTEST_MEMORYCHECKCOMMAND_OPTIONS  ${VALGRIND_COMMAND_OPTIONS} )
ENDIF()


# Clear the binary directory and create an initial cache
EXECUTE_PROCESS( COMMAND ${CMAKE_COMMAND} -E remove -f CMakeCache.txt )
EXECUTE_PROCESS( COMMAND ${CMAKE_COMMAND} -E remove_directory CMakeFiles )
FILE(WRITE "${CTEST_BINARY_DIRECTORY}/CMakeCache.txt" "CTEST_TEST_CTEST:BOOL=1")


# Set the configure options
SET( CTEST_OPTIONS )
SET( CTEST_OPTIONS "-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}" )
SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DCMAKE_C_COMPILER:PATH=${CC};-DCMAKE_C_FLAGS='${C_FLAGS}';" )
SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DCMAKE_CXX_COMPILER:PATH=${CXX};-DCMAKE_CXX_FLAGS='${CXX_FLAGS}'" )
SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DMPI_COMPILER:BOOL=true;-DMPIEXEC=${MPIEXEC};-DUSE_EXT_MPI_FOR_SERIAL_TESTS:BOOL=true")
IF ( USE_TIMER )
    SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DUSE_TIMER:BOOL=true;-DTIMER_DIRECTORY='${TIMER_DIRECTORY}'" )
ELSE()
    SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DUSE_TIMER:BOOL=false" )
ENDIF()
IF ( USE_CUDA )
    SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DUSE_CUDA:BOOL=true;-DCUDA_NVCC_FLAGS='${CUDA_FLAGS}';-DCUDA_HOST_COMPILER=${CUDA_HOST_COMPILER};-DLIB_TYPE=SHARED" )
ELSE()
    SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DUSE_CUDA:BOOL=false" )
ENDIF()
SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DLDLIBS:STRING=\"${LDLIBS}\"" )
SET( CTEST_OPTIONS "${CTEST_OPTIONS};-DENABLE_GCOV:BOOL=${ENABLE_GCOV}" )

# Configure and run the tests
SET( CTEST_SITE ${HOSTNAME} )
CTEST_START("${CTEST_DASHBOARD}")
CTEST_UPDATE()
CTEST_CONFIGURE(
    BUILD   ${CTEST_BINARY_DIRECTORY}
    SOURCE  ${CTEST_SOURCE_DIRECTORY}
    OPTIONS "${CTEST_OPTIONS}"
)
CTEST_BUILD()
IF ( USE_VALGRIND_MATLAB )
    CTEST_TEST( INCLUDE MATLAB  PARALLEL_LEVEL ${N_PROCS} )
ELSEIF ( USE_VALGRIND )
    # CTEST_MEMCHECK( EXCLUDE "(WEEKLY|procs|example--)"  PARALLEL_LEVEL ${N_PROCS} )
    CTEST_MEMCHECK( EXCLUDE "(WEEKLY|example--)"  PARALLEL_LEVEL ${N_PROCS} )
ELSEIF ( RUN_WEEKLY )
    CTEST_TEST( INCLUDE "(WEEKLY|example--)"  PARALLEL_LEVEL ${N_PROCS} )
ELSE()
    CTEST_TEST( EXCLUDE "(WEEKLY|example--)"  PARALLEL_LEVEL ${N_PROCS} )
ENDIF()
IF( ENABLE_GCOV )
    CTEST_COVERAGE()
ENDIF()


# Submit the results to oblivion
SET( CTEST_DROP_METHOD "http" )
SET( CTEST_DROP_SITE "mberrill.myqnapcloud.com" )
SET( CTEST_DROP_LOCATION "/CDash/submit.php?project=LBPM-WIA" )
SET( CTEST_DROP_SITE_CDASH TRUE )
SET( DROP_SITE_CDASH TRUE )
CTEST_SUBMIT()


# Write a message to test for success in the ctest-builder
MESSAGE( "ctest_script ran to completion" )


