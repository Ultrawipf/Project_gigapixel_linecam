# Set SOURCES variable
cmake_minimum_required(VERSION 3.16)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(mock_wiringpi LANGUAGES CXX)

set(SOURCES
  wiringPi.cpp wiringSerial.cpp
)

# Set HEADERS_PUBLIC variable (public headers, included in the library)
set(HEADERS_PUBLIC
  wiringPi.h wiringSerial.h
)

add_library(mock_wiringpi ${SOURCES} ${HEADERS_PUBLIC})

# Set HEADERS_PRIVATE variable, if needed.
# (these private headers won't be included in the library)
# set(HEADERS_PRIVATE
#     foo_private.h
# )

#include(${PROJECT_SOURCE_DIR}/cmake/LibraryConfig.cmake)
