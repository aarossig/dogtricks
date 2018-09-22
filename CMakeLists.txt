#
# dogtricks
#

# Project Settings #############################################################

# Setup CMake and language requirements.
cmake_minimum_required(VERSION 3.1)
set(CMAKE_CXX_STANDARD 17)

# Include directories
include_directories(src)

# Project name and sources
project(dogtricks)
add_subdirectory(src)
