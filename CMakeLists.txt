cmake_minimum_required(VERSION 3.10)
project(NormalizedGrammars)

# Set C++ standard (optional but recommended)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add source files automatically from src/
file(GLOB SRC_FILES "src/*.cpp")

# Create executable
add_executable(${PROJECT_NAME} ${SRC_FILES})

# Include headers from src/
target_include_directories(${PROJECT_NAME} PRIVATE src)