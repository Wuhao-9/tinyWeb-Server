cmake_minimum_required(VERSION 3.0)

project(tiny-WebServer)

# set(
    # CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g"
# )

set(CMAKE_CXX_STANDARD 17)

include_directories(/usr/include/mysql)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR})

file(GLOB SRCS "${CMAKE_SOURCE_DIR}/*.cpp")

message(STATUS ${SRCS})

add_executable(${CMAKE_PROJECT_NAME} ${SRCS})

target_link_libraries(${CMAKE_PROJECT_NAME} 
    pthread
    mysqlclient
)