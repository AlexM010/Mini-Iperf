cmake_minimum_required(VERSION 3.10)

project(mini_iperf C)

set(CMAKE_C_STANDARD 99)

# Add source files
file(GLOB SOURCES "src/*.c")

# Create the executable
add_executable(mini_iperf ${SOURCES})

target_link_libraries(mini_iperf pthread)