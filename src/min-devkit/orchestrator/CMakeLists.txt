cmake_minimum_required(VERSION 3.0)

# Name of the project
set(PROJECT_NAME "mcp.orchestrator")

# Use C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Creating the module
include(${CMAKE_CURRENT_SOURCE_DIR}/../../../script/max-pretarget.cmake)

# Add all CPP files to the build
file(GLOB_RECURSE PROJECT_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)

# Create the module
include(${CMAKE_CURRENT_SOURCE_DIR}/../../../script/max-posttarget.cmake)
