#[[
LiteCore REST library CMake project

This project builds a simple REST API for LiteCore to use in conjunction with the shared
library tests.  It is *not* meant to be used anywhere else, so don't do it!
]]#

cmake_minimum_required (VERSION 3.9)
cmake_policy(VERSION 3.9)
project (LiteCoreREST)

### BUILD SETTINGS:

set(COMPILE_FLAGS   "${COMPILE_FLAGS}   -Wall -Werror")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

### SOURCES:

set(
    ALL_SRC_FILES
    c4Listener+RESTFactory.cc
    c4Listener.cc
    Listener.cc
    netUtils.cc
    Request.cc
    Response.cc
    RESTListener+Handlers.cc
    RESTListener+Replicate.cc
    RESTListener.cc
    Server.cc
    EE/RESTSyncListener_stub.cc
)

### STATIC LIBRARY:

add_library(LiteCoreREST_Static STATIC ${ALL_SRC_FILES})
target_include_directories(
    LiteCoreREST_Static PRIVATE
    .
    ../C/include
    ../C
    ../LiteCore/Support
    ../Crypto
    ../Networking
    ../Networking/BLIP
    ../Networking/HTTP
    ../Networking/WebSockets
    ../Replicator
    ../vendor/fleece/API
    ../vendor/fleece/Fleece/Support
    ../vendor/mbedtls/include
    ../vendor/sockpp/include
    ../vendor/fleece/vendor/date/include
)
if(BUILD_ENTERPRISE)
    target_include_directories(
        LiteCoreREST_Static PRIVATE
        ../../vendor/couchbase-lite-core-EE/Listener
    )
endif()

target_compile_definitions(
    LiteCoreREST_Static PRIVATE
    HAS_UNCAUGHT_EXCEPTIONS # date.h use std::uncaught_exceptions instead of std::uncaught_exception
)

target_link_libraries(
    LiteCoreREST_Static PUBLIC
    LiteCoreStatic
)

setup_rest_build()
