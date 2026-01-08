cmake_minimum_required(VERSION 3.18)

project(gomoku VERSION 1.0.0 LANGUAGES CXX)

find_library(MYSQL_LIB
    NAMES mysqlclient
    PATHS /usr/lib /usr/lib/x86_64-linux-gnu 
)

set(CMAKE_CXX_STANDARD 11)

file(GLOB SRC_FILES "./*.cc")

find_package(jsoncpp CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)

add_executable(main ${SRC_FILES})

target_include_directories(main PRIVATE /usr/include/mysql)
target_include_directories(main PRIVATE /usr/local/include)

target_link_libraries(main PRIVATE ${MYSQL_LIB} JsonCpp::JsonCpp Boost::system Threads::Threads)


