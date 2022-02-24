cmake_minimum_required (VERSION 2.8)
project (teec)

# Add source files
set(LIB_SRCS
    ../libteec_vendor/tee_client_api.c
    ../libteec_vendor/tee_client_ext_api.c
    ../libteec_vendor/tee_client_app_load.c
    ../libteec_vendor/tee_client_socket.c
    ../teecd/secfile_load_agent.c
    ../libteec_vendor/tee_session_pool.c
)
set(APP_SRCS
    ./tee_agent.c
    ./tee_ca_daemon.c
    ./secfile_load_agent.c
    ./fs_work_agent.c
    ./misc_work_agent.c
    ./tee_ca_auth.c
    ./system_ca_auth.c
    ../authentication/tee_get_native_cert.c
    ../authentication/tee_auth_common.c
)

# Add header file include directories
include_directories(
    ../../include
    ../../include/cloud
    ../../ext_include
    ../../libboundscheck/include
    ../../include
    ../inc
    ./
    ../authentication
)

# Set compile options
set(CMAKE_TEEC_FLAGS "-DSEC_STROAGE_DATA_KUNPENG_PATH -DSECURITY_AUTH_ENHANCE")
set(CMAKE_TEEC_FLAGS "${CMAKE_TEEC_FLAGS} -shared")
set(CMAKE_TEEC_FLAGS "${CMAKE_TEEC_FLAGS} -Werror -Wall -Wextra -fstack-protector-all -Wl,-z,relro,-z,now,-z,noexecstack -s -fPIC -D_FORTIFY_SOURCE=2 -O2")

# Set options for teecd
set(CMAKE_TEECD_FLAGS "-DSEC_STROAGE_DATA_KUNPENG_PATH -D_GNU_SOURCE -DSECURITY_AUTH_ENHANCE -DCONFIG_AGENT_FS")
set(CMAKE_TEECD_FLAGS "${CMAKE_TEECD_FLAGS} -Werror -Wall -Wextra -fstack-protector-all -Wl,-z,relro,-z,now,-z,noexecstack -s -fPIE -pie -D_FORTIFY_SOURCE=2 -O2")

# Set libboundscheck lib path
link_directories(${CMAKE_PROJECT_ROOT_PATH}/libboundscheck/lib)

# Generate .so file
add_library(teec SHARED ${LIB_SRCS})
target_link_libraries(teec boundscheck)
set_target_properties(teec PROPERTIES COMPILE_FLAGS ${CMAKE_TEEC_FLAGS})

# Generate execute file
add_executable(teecd ${APP_SRCS})
target_link_libraries(teecd boundscheck pthread crypto)
set_target_properties(teecd PROPERTIES COMPILE_FLAGS ${CMAKE_TEECD_FLAGS})
 
# Copy teec,teecd and boundscheck to dist
add_custom_command(TARGET teec
    POST_BUILD
    COMMAND mkdir -p ${CMAKE_PROJECT_ROOT_PATH}/dist 
    COMMAND cp -f ${CMAKE_PROJECT_ROOT_PATH}/libboundscheck/lib/libboundscheck.so ${CMAKE_PROJECT_ROOT_PATH}/dist 
    COMMAND mv ./libteec.so ${CMAKE_PROJECT_ROOT_PATH}/dist
)
add_custom_command(TARGET teecd
    POST_BUILD
    COMMAND mv ./teecd ${CMAKE_PROJECT_ROOT_PATH}/dist
)
