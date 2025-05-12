cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)

set(PROJECT_NAME shitbrix)
set(SRC_DIR src)
set(TEST_DIR tests)

project(${PROJECT_NAME} CXX)

# Define Release by default
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
  message(STATUS "Build type not specified: Use Release by default.")
endif(NOT CMAKE_BUILD_TYPE)

# MSVC compatibility
add_definitions(
   -DNOMINMAX
   -D_CRT_SECURE_NO_WARNINGS
   -DUNICODE
   -D_UNICODE
)

# Outputs depending on Debug or Release
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE}")
set(CMAKE_EXECUTABLE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}")

# Dependencies
# find_package(Threads REQUIRED)
# SET(Threads_FOUND TRUE) # pthreads check is currently broken, maybe
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_ttf REQUIRED)
find_package(ENet REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS} ${SDL2_IMAGE_INCLUDE_DIRS} ${ENet_INCLUDE_DIRS} ${SDL2_TTF_INCLUDE_DIRS})

# option(BUILD_DEPENDS "Build other CMake project." ON)
# Dependencies : disable BUILD_DEPENDS to link with lib already build.
# if(BUILD_DEPENDS)
#    add_subdirectory(ext/enet ${CMAKE_BINARY_DIR}/enet)
#    add_subdirectory(ext/sdl ${CMAKE_BINARY_DIR}/SDL)
#    add_subdirectory(ext/sdl_image ${CMAKE_BINARY_DIR}/SDL_image)
#    add_subdirectory(ext/sdl_ttf ${CMAKE_BINARY_DIR}/SDL_ttf)
# else()
#    link_directories(dependencies/enet/build/)
#    link_directories(dependencies/SDL/build/)
#    link_directories(dependencies/SDL_image/build/)
#    link_directories(dependencies/SDL_ttf/build/)
# endif()

# Compiler Flags
if(MSVC)
   set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /W3 /EHsc")
   set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /W3 /EHsc")
else()
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++1z")
   if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
       set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
   endif()
endif(MSVC)

# Source Files
##############

include_directories(${SRC_DIR})

file(GLOB INCLUDE_FILES ${SRC_DIR}/*.hpp)
file(GLOB SOURCE_FILES_CORE ${SRC_DIR}/*.cpp)
list(FILTER SOURCE_FILES_CORE EXCLUDE REGEX ".*/main.cpp$")
set(SOURCE_FILES_MAIN ${SRC_DIR}/main.cpp)

# Core library target
#####################

add_library(${PROJECT_NAME}_core STATIC
    ${INCLUDE_FILES}
    ${SOURCE_FILES_CORE}
)

target_link_libraries(${PROJECT_NAME}_core
    ${SDL2_LIBRARIES} ${SDL2_IMAGE_LIBRARIES} ${SDL2_TTF_LIBRARIES} ${ENet_LIBRARIES} stdc++fs
)

# Main executable target
########################

add_executable(${PROJECT_NAME}
    ${SOURCE_FILES_MAIN}
)

target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME}_core)
# This is somehow required because cmake does not figure it out
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)

# Test executable target
########################

enable_testing()

file(GLOB TEST_SOURCES
    ${TEST_DIR}/*.cpp
)

add_executable(${PROJECT_NAME}_tests
    ${TEST_SOURCES}
)

# Local GTest/GMock included
add_subdirectory(ext/googletest)

target_include_directories(${PROJECT_NAME}_tests PRIVATE ${TEST_DIR})
target_link_libraries(${PROJECT_NAME}_tests
    ${PROJECT_NAME}_core
    gmock_main
)

add_test(NAME all_tests COMMAND ${PROJECT_NAME}_tests)
