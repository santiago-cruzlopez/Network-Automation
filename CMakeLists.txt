cmake_minimum_required(VERSION 3.5)
project(Networking-DPDK
    VERSION 1.0
    DESCRIPTION ""
    LANGUAGES C CXX
)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(OSBitness 32)
if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(OSBitness 64)
endif()

set(FullOutputDir "${CMAKE_SOURCE_DIR}/bin/${CMAKE_SYSTEM_NAME}${OSBitness}/${CMAKE_BUILD_TYPE}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${FullOutputDir}/static_libs")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${FullOutputDir}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${FullOutputDir}")


# Create executable
add_executable(${PROJECT_NAME} 
    main.cpp
)

# Configure include directories
target_include_directories(${PROJECT_NAME} PRIVATE 
    "${CMAKE_SOURCE_DIR}/src"
)

# Link libraries
target_link_libraries(${PROJECT_NAME})

# Platform-specific configurations for DeckLink
if(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} dl pthread)
endif()