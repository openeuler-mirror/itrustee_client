cmake_minimum_required (VERSION 2.8)
project (tlogcat)

# Add source files
file(GLOB SRCS "./*.c")

# Add header file include directories
include_directories(
    ../../include
    ../../include/cloud
    ../../ext_include
    ../../libboundscheck/include
    ../../include
    ../inc
    ./
)

# Set compile options
set(CMAKE_TLOGCAT_FLAGS "-Werror -Wextra -DTLOGCAT_SYS_LOG")
set(CMAKE_TLOGCAT_FLAGS "${CMAKE_TLOGCAT_FLAGS} -DTEE_KTRACE_DUMP -DLOG_PATH_TEE=\\\"/var/log/tee/\\\" -DLOG_PATH_BASE=\\\"/var/log/\\\"")
set(CMAKE_TLOGCAT_FLAGS "${CMAKE_TLOGCAT_FLAGS} -DLOG_TMPPATH_TEE=\\\"/var/log/tee/_tmp/\\\" -DAID_SYSTEM=0")
set(CMAKE_TLOGCAT_FLAGS "${CMAKE_TLOGCAT_FLAGS} -Werror -Wall -Wextra -fstack-protector-all -Wl,-z,relro,-z,now,-z,noexecstack -s -fPIE -pie -D_FORTIFY_SOURCE=2 -O2")

# Set libboundscheck lib path
link_directories(${CMAKE_PROJECT_ROOT_PATH}/libboundscheck/lib)

# Generate execute file
add_executable(tlogcat ${SRCS})
target_link_libraries(tlogcat boundscheck z)
set_target_properties(tlogcat PROPERTIES COMPILE_FLAGS ${CMAKE_TLOGCAT_FLAGS})
 
# Copy tlogcat to dist
add_custom_command(TARGET tlogcat
    POST_BUILD
    COMMAND mkdir -p ${CMAKE_PROJECT_ROOT_PATH}/dist 
    COMMAND mv ./tlogcat ${CMAKE_PROJECT_ROOT_PATH}/dist
)
