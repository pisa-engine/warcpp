cmake_minimum_required(VERSION 3.0)
project(warcpp)

if (NOT CMAKE_BUILD_TYPE)
        message(STATUS "No build type selected, default to Release")
        set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()
MESSAGE( STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE} )

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(WARCPP_ENABLE_TESTING "Enable testing of the library." ON)
option(WARCPP_BUILD_TOOL "Build cmd tool." ON)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    if (CXX_COMPILER_VERSION VERSION_LESS 4.7)
        message(STATUS "GCC version must be at least 4.7!")
    endif()
    set (CMAKE_CXX_FLAGS_RELEASE "-Wall -Wcast-align -Ofast -lm  -DNDEBUG -DHAVE_CXX0X -march=native")
    set (CMAKE_CXX_FLAGS_DEBUG   "-Wall -Wcast-align -ggdb  -lm  -DHAVE_CXX0X -march=native")
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
    if (CXX_COMPILER_VERSION VERSION_LESS 4.2.1)
        message(STATUS  "Clang version must be at least 4.2.1!" )
    endif()
    set (CMAKE_CXX_FLAGS_RELEASE "-Wall -Wcast-align -O3 -DNDEBUG -DHAVE_CXX0X -msse4.1 -march=native")
    set (CMAKE_CXX_FLAGS_DEBUG   "-Wall -Wcast-align -ggdb -DHAVE_CXX0X -msse4.1 -march=native")
else ()
    message(FATAL_ERROR "Please, use GCC or Clang compiler!")
endif()

add_subdirectory(external)

include_directories(include)
add_library(warcpp INTERFACE)
target_include_directories(warcpp INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
)

include(CTest)
if (WARCPP_ENABLE_TESTING AND BUILD_TESTING)
    enable_testing()
    add_subdirectory(test)
endif()

if (WARCPP_BUILD_TOOL)
    if(NOT TARGET CLI11)
        # Add CLI11
        set(CLI11_TESTING OFF CACHE BOOL "skip CLI11 testing")
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/external/CLI11 EXCLUDE_FROM_ALL)
    endif()
    add_subdirectory(src)
endif()
