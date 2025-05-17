cmake_minimum_required(VERSION 3.16)

project(linearcam_pi LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#if(DUMMY)
#    add_compile_definitions(DUMMY)
#add_library(mock_wiringpi
#    wiringpi_dummy/wiringPi.cpp wiringpi_dummy/wiringPi.h
#    wiringpi_dummy/wiringSerial.cpp wiringpi_dummy/wiringSerial.h)
#include_directories(wiringpi_dummy)
#endif()
#if (DEBUG_MODE)
#else (DEBUG_MODE)
#    add_compile_definitions("DUMMY")
#endif(DEBUG_MODE)


add_executable(linearcam_pi main.cpp
    argparse.hpp
    tmc2209_motor.h tmc2209_motor.cpp
    constants_pi.h
    motors.h motors.cpp
)
add_library(linearcam_pi_lib     argparse.hpp
    tmc2209_motor.h tmc2209_motor.cpp
    constants_pi.h
    motors.h motors.cpp)

#add_dependencies(linearcam_pi mock_wiringpi)
#target_include_directories(linearcam_pi PUBLIC wiringpi_dummy)
include(CMakePrintHelpers)
cmake_print_variables(DUMMY)
if(${DUMMY})
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/wiringpi_dummy)
add_compile_definitions(DUMMY)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/wiringpi_dummy)
#add_dependencies(linearcam_pi mock_wiringpi)
target_link_libraries(linearcam_pi PUBLIC mock_wiringpi)
target_link_libraries(linearcam_pi_lib PUBLIC mock_wiringpi)

else()

target_link_libraries(linearcam_pi PRIVATE wiringPi)
target_link_libraries(linearcam_pi_lib PRIVATE wiringPi)
endif()


include(GNUInstallDirs)
install(TARGETS linearcam_pi
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
