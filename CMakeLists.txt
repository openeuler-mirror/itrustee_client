cmake_minimum_required (VERSION 2.8)
project (itrustee_client)
cmake_policy (SET CMP0037 OLD)

# Set variable
set(CMAKE_PROJECT_ROOT_PATH ${PROJECT_SOURCE_DIR})
set(CMAKE_FLAGS_PRIMARY ${CMAKE_C_FLAGS})

# Add libboundscheck
add_subdirectory(./libboundscheck)

# Add teecd
add_subdirectory(src/teecd)

# Add tlogcat
add_subdirectory(src/tlogcat)

# Clean output
add_custom_target(clean
    COMMAND -rm -rf ${CMAKE_PROJECT_ROOT_PATH}/libboundscheck/lib ${CMAKE_PROJECT_ROOT_PATH}/dist
    COMMAND @echo "clean up"
)
