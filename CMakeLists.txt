cmake_minimum_required(VERSION 3.0.2)

set(VERSION_MAJOR 1)
set(VERSION_MINOR 0)
set(VERSION_PATCH 0)

project(is_vim VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})

FILE(GLOB SRC "${CMAKE_CURRENT_LIST_DIR}/*.c")

add_executable(is_vim ${SRC})
