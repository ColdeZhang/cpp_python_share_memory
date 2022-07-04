cmake_minimum_required(VERSION 3.22)
project(cpp_python)
set(CMAKE_CXX_STANDARD 17)

link_directories(external/opencv_build/lib64)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/opencv_build/include/opencv4)

SET(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "${CMAKE_SOURCE_DIR}/external/opencv_build/lib64/cmake")

find_package(OpenCV REQUIRED)


add_executable(cpp_python test.cpp cpp_src/share_memory.cpp cpp_src/share_memory.h)

add_library(sharememory SHARED cpp_src/share_memory.h cpp_src/share_memory.cpp)

target_link_directories(cpp_python
        PRIVATE external/opencv_build/lib64)

target_link_libraries(cpp_python ${OpenCV_LIBS})