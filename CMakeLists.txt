cmake_minimum_required(VERSION 2.6)
project(avbot)
#SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
option(ENABLE_STATIC "link to boost staticaly" ON)
option(ENABLE_TEST "build xmpp/pop3 test" OFF)
option(ENABLE_CXX11 "build avbot with cxx11" OFF)
option(ENABLE_LTO "build avbot with LTO" OFF)
option(ENABLE_LIBSTDCXX_STATIC "link to static libstdc++" OFF)
option(ENABLE_LIBGCC_STATIC "link to static libgcc" OFF)

if(WIN32)
set(ENABLE_LIBGCC_STATIC ON)
set(ENABLE_LIBSTDCXX_STATIC ON)
set(ENABLE_STATIC ON)
endif()

if(WIN32)
add_definitions(-D_WIN32_WINNT=0x0501 -DWINVER=0x0501)
endif()

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -g0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g0")

set(CMAKE_C_FLAGS_TEST "-Og -g")
set(CMAKE_CXX_FLAGS_TEST "-Og -g0")

if(CMAKE_BUILD_TYPE EQUAL "TEST")
set(ENABLE_STATIC ON)
endif()

if(ENABLE_STATIC)
set(Boost_USE_STATIC_LIBS        ON)
set(Boost_USE_STATIC_RUNTIME    ON)
endif()

if(ENABLE_LIBSTDCXX_STATIC)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ ")
set(Boost_USE_STATIC_LIBS        ON)
set(Boost_USE_STATIC_RUNTIME    ON)
endif()

if(ENABLE_LIBGCC_STATIC)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc ")
endif()

if(ENABLE_CXX11)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()
if(ENABLE_LTO)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
	set(CMAKE_LD_FLAGS "${CMAKE_LD_FLAGS} -flto -Wl,-flto")
endif()

find_package(Boost 1.54 COMPONENTS log log_setup date_time filesystem system program_options regex locale)

if(NOT Boost_FOUND)
	set(Boost_USE_STATIC_LIBS        OFF)
	set(Boost_USE_STATIC_RUNTIME    OFF)
	find_package(Boost 1.54 REQUIRED COMPONENTS log log_setup date_time filesystem system program_options regex locale)
	if(NOT Boost_FOUND )
		message(FATAL_ERROR "need boost >= 1.54")
	endif()
endif()

# add_definitions(-DBOOST_LOG_NO_THREADS)

find_package(Threads)

if(WIN32 AND Boost_USE_STATIC_LIBS)
add_definitions(-DBOOST_THREAD_USE_LIB)
add_definitions(-DBOOST_FILESYSTEM_STATIC_LINK)
add_definitions(-DBOOST_ALL_STATIC_LINK)
endif()

find_package(OpenSSL)

add_definitions(-DQQBOT_VERSION=\"7.1\")
add_definitions(-DAVHTTP_ENABLE_OPENSSL)

set(WITH_EMPTY OFF)
set(WITH_SQLITE ON)
set(WITH_BOOST ON)
set(WITH_FIREBIRD OFF)
set(WITH_MYSQL OFF)
set(WITH_ODBC OFF)
set(WITH_ORACLE OFF)
set(WITH_POSTGRESQL OFF)
set(SOCI_STATIC ON)
set(SOCI_TESTS OFF)
set(WITH_DB2 OFF)
add_subdirectory(soci/src)

include_directories(${Boost_INCLUDE_DIRS})
include_directories(${PROJECT_SOURCE_DIR})
include_directories(avboost/include)
include_directories(avproxy/include)
include_directories(avhttp/include)
include_directories(deCAPTCHA/include)

add_subdirectory(libwebqq)
if(ENABLE_TEST)
add_subdirectory(libirc)
endif()
add_subdirectory(libxmpp)
add_subdirectory(libmailexchange)
add_subdirectory(deCAPTCHA)
add_subdirectory(libavbot)
add_subdirectory(libavlog)
add_subdirectory(extension)



add_executable(avbot avbot_rpc_server.cpp main.cpp botctl.cpp input.cpp avbot.rc)

if(WIN32)
add_subdirectory(gui)
target_link_libraries(avbot avbotgui)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static ")
target_link_libraries(avbot -liconv -lssl -lcrypto -lcrypt32 -lws2_32 -lcomctl32 -lmswsock)
endif()


if(APPLE)
target_link_libraries(avbot -liconv)
endif()

target_link_libraries(avbot libavbot avlog extension)

target_link_libraries(avbot ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

install(TARGETS avbot RUNTIME DESTINATION bin)

if( NOT Boost_USE_STATIC_RUNTIME )
	message(STATUS "dynamic link to boost is not recomanded")
endif()

check_library_exists(rt clock_gettime "" HAVE_CLOCK_GETTIME)
if(HAVE_CLOCK_GETTIME)
        target_link_libraries(avbot -lrt)
endif(HAVE_CLOCK_GETTIME)
