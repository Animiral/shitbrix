cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)

################### Variables. ####################
# Change if you want modify path or other values. #
###################################################

set(PROJECT_NAME shitbrix)
# Folders files
set(SRC_DIR src)

############## CMake Project ################
#        The main options of project        #
#############################################

project(${PROJECT_NAME} CXX)
# ENABLE_LANGUAGE(C) # required by FindSDL2
# list(APPEND CMAKE_CXX_SOURCE_FILE_EXTENSIONS c) # maybe also required for threads?

# Define Release by default.
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
  message(STATUS "Build type not specified: Use Release by default.")
endif(NOT CMAKE_BUILD_TYPE)

# Definition of Macros
add_definitions(
   -DNOMINMAX
   -D_CRT_SECURE_NO_WARNINGS
   -DUNICODE
   -D_UNICODE
)

############## Artefacts Output #################
# Defines outputs , depending Debug or Release. #
#################################################

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE}")
set(CMAKE_EXECUTABLE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}")

################### Dependencies ##################
# Add Dependencies to project.                    #
###################################################

#find_package(Threads REQUIRED)
SET(Threads_FOUND TRUE) # pthreads check is currently broken, maybe
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(enet REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS} ${SDL2_IMAGE_INCLUDE_DIRS} ${ENet_INCLUDE_DIRS})

# option(BUILD_DEPENDS
#    "Build other CMake project."
#    ON
# )

# Dependencies : disable BUILD_DEPENDS to link with lib already build.
# if(BUILD_DEPENDS)
#    add_subdirectory(ext/enet ${CMAKE_BINARY_DIR}/enet)
#    add_subdirectory(ext/sdl ${CMAKE_BINARY_DIR}/SDL)
#   add_subdirectory(ext/sdl_image ${CMAKE_BINARY_DIR}/SDL_image)
# else()
#    link_directories(dependencies/enet/build/)
#    link_directories(dependencies/SDL/build/)
#    link_directories(dependencies/SDL_image/build/)
# endif()

################# Flags ################
# Defines Flags for Windows and Linux. #
########################################

if(MSVC)
   set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /W3 /EHsc")
   set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /W3 /EHsc")
endif(MSVC)
if(NOT MSVC)
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++1z")
   if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
       set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
   endif()
endif(NOT MSVC)

################ Files ################
#   --   Add files to project.   --   #
#######################################

include_directories(${SRC_DIR})

set(INCLUDE_FILES
    ${SRC_DIR}/asset.hpp
    ${SRC_DIR}/audio.hpp
    ${SRC_DIR}/context.hpp
    ${SRC_DIR}/director.hpp
    ${SRC_DIR}/draw.hpp
    ${SRC_DIR}/enet_helper.hpp
    ${SRC_DIR}/error.hpp
    ${SRC_DIR}/event.hpp
    ${SRC_DIR}/game_loop.hpp
    ${SRC_DIR}/globals.hpp
    ${SRC_DIR}/input.hpp
    ${SRC_DIR}/logic.hpp
    ${SRC_DIR}/network.hpp
    ${SRC_DIR}/options.hpp
    ${SRC_DIR}/replay.hpp
    ${SRC_DIR}/screen.hpp
    ${SRC_DIR}/sdl_helper.hpp
    ${SRC_DIR}/stage.hpp
)

set(SOURCE_FILES
    ${SRC_DIR}/asset.cpp
    ${SRC_DIR}/audio.cpp
    ${SRC_DIR}/context.cpp
    ${SRC_DIR}/director.cpp
    ${SRC_DIR}/draw.cpp
    ${SRC_DIR}/enet_helper.cpp
    ${SRC_DIR}/error.cpp
    ${SRC_DIR}/event.cpp
    ${SRC_DIR}/game_loop.cpp
    ${SRC_DIR}/globals.cpp
    ${SRC_DIR}/input.cpp
    ${SRC_DIR}/logic.cpp
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/network.cpp
    ${SRC_DIR}/options.cpp
    ${SRC_DIR}/replay.cpp
    ${SRC_DIR}/screen.cpp
    ${SRC_DIR}/sdl_helper.cpp
    ${SRC_DIR}/stage.cpp
)

source_group(include FILES ${INCLUDE_FILES})
source_group(source FILES ${SOURCE_FILES})

# Add library to build.
# add_library(${PROJECT_NAME} STATIC
#    ${INCLUDE_FILES}
#    ${SRC_FILES}
# )

add_executable(${PROJECT_NAME}
   ${INCLUDE_FILES}
   ${SOURCE_FILES}
)

# This is somehow required because cmake does not figure it out
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)

# Link with other dependencies.
target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES} ${SDL2_IMAGE_LIBRARIES} ${ENet_LIBRARIES})

# set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)
