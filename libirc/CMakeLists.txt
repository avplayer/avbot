cmake_minimum_required(VERSION 2.6)
project(libirc)

find_package(Boost 1.54 REQUIRED COMPONENTS date_time system regex log thread)

include_directories(${Boost_INCLUDE_DIRS})

add_library(irc STATIC irc.cpp)

add_executable(irc3test EXCLUDE_FROM_ALL test.cpp )
target_link_libraries(irc3test irc ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
