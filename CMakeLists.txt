cmake_minimum_required(VERSION 3.0.0)
project(NETCAD)

set(OSBitness 32)
if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(OSBitness 64)
endif()

set(FullOutputDir "${CMAKE_SOURCE_DIR}/bin/${CMAKE_SYSTEM_NAME}${OSBitness}/${CMAKE_BUILD_TYPE}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${FullOutputDir}/static_libs")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${FullOutputDir}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${FullOutputDir}")

# Find required packages
find_package(SDL2 REQUIRED)

# Define DeckLink SDK path: DeckLinkAPI.h
set(DECKLINK_SDK_PATH "/home/santi_cruz/Documents/Network-Automation/Broadcasting_Media/Linux")

# DeckLink SDK source Files
set(DECKLINK_SOURCES
    "${DECKLINK_SDK_PATH}/include/DeckLinkAPIDispatch.cpp"
)

set(APP_SOURCES
    "${CMAKE_SOURCE_DIR}/src_sdk/DeckLinkManager.cpp"
    "${CMAKE_SOURCE_DIR}/src_sdk/CaptureDelegate.cpp"
)

# Create executable
add_executable(${PROJECT_NAME} 
    DeckLink_Capture.cpp
    ${DECKLINK_SOURCES}
    ${APP_SOURCES}
)

# Configure include directories
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${SDL2_INCLUDE_DIRS}
    "${DECKLINK_SDK_PATH}/include"
    "${CMAKE_SOURCE_DIR}/src_sdk"
)

# Link libraries
target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES})

# Platform-specific configurations for DeckLink
if(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} dl pthread)
endif()