cmake_minimum_required(VERSION 3.16)

project(linearcam_pi LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(${DUMMY})
add_library(mock_wiringpi
    wiringpi_dummy/wiringPi.cpp wiringpi_dummy/wiringPi.h
    wiringpi_dummy/wiringSerial.cpp wiringpi_dummy/wiringSerial.h)

endif()
#if (DEBUG_MODE)
#else (DEBUG_MODE)
#    add_compile_definitions("DUMMY")
#endif(DEBUG_MODE)


add_executable(linearcam_pi main.cpp
    argparse.hpp
    tmc2209_motor.h tmc2209_motor.cpp
    constants_pi.h)

#add_dependencies(linearcam_pi mock_wiringpi)
#target_include_directories(linearcam_pi PUBLIC wiringpi_dummy)
if(${DUMMY})

target_link_libraries(linearcam_pi PRIVATE mock_wiringpi)
else()
target_link_libraries(linearcam_pi PRIVATE wiringPi)
endif()


include(GNUInstallDirs)
install(TARGETS linearcam_pi
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
