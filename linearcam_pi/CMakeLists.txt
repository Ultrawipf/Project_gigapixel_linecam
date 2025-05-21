cmake_minimum_required(VERSION 3.16)

project(linearcam_pi LANGUAGES CXX C)

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

#set(PIOPATH /home/pi/git/utils/piolib/)
#link_directories(${PIOPATH})
#include_directories(/home/pi/git/utils/piolib/include/)
#add_custom_command(
#  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/pio_ccd.h
#  COMMAND pioasm pio_ccd.pio pio_ccd.h
#  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
#  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/pio_ccd.pio
#)

#file(GLOB CCD_FILES "${CMAKE_CURRENT_LIST_DIR}/../epson_ccd_pio/*.c")

if(${DUMMY})
    message("Dummy build. no CCD")
else()
    include(../epson_ccd_pio/CMakeCCDFiles_piolib.txt)
    #include(/home/pi/git/utils/piolib/CMakeLists.txt)

endif()



add_executable(linearcam_pi main.cpp argparse.hpp)
add_library(linearcam_pi_lib
    tmc2209_motor.h tmc2209_motor.cpp
    constants_pi.h
    cameracontrol.h cameracontrol.cpp
    ccd_ctrl.cpp ccd_ctrl.h pio_ccd.h
    ccd_constants.h
    ${CCD_SOURCES}  ${CCD_HEADERS_PUBLIC}
)

#include_directories("${CMAKE_CURRENT_LIST_DIR}/../epson_ccd_pio")

#add_dependencies(linearcam_pi mock_wiringpi)
#target_include_directories(linearcam_pi PUBLIC wiringpi_dummy)

if(${DUMMY})
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/wiringpi_dummy)
    add_compile_definitions(DUMMY)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/wiringpi_dummy)
    #add_dependencies(linearcam_pi mock_wiringpi)
    target_link_libraries(linearcam_pi PRIVATE mock_wiringpi)
    target_link_libraries(linearcam_pi_lib PRIVATE mock_wiringpi)

else()

    if(${PIOLIB})

        #include(../epson_ccd_pio/CMakeCCDFiles.txt)
        add_compile_definitions(PIOLIB)
        #add_dependencies(linearcam_pi pio)
        #add_dependencies(linearcam_pi_lib pio)
      #set(PIOPATH /home/pi/git/utils/piolib/)
      #set(HEADERS_PUBLIC ccd_constants.h)
      message("CCD SOURCES: ${CCD_SOURCES}")
        #link_directories(${PIOPATH})
        # INFO MUST MAKE AND INSTALL SHARED libpio.so first
        target_link_libraries(linearcam_pi PRIVATE "/usr/local/lib/libpio.so.0")
        target_link_libraries(linearcam_pi_lib PRIVATE "/usr/local/lib/libpio.so.0")
        #add_library (pio OBJECT ${PIOPATH}/piolib.c ${PIOPATH}/pio_rp1.c)
        #   target_include_directories(pio PUBLIC ${PIOINCLUDEPATH})

        target_include_directories(linearcam_pi PUBLIC ${PIOINCLUDEPATH})
        target_include_directories(linearcam_pi_lib PUBLIC ${PIOINCLUDEPATH})

        #target_link_libraries(linearcam_pi PRIVATE pio)
        #target_link_libraries(linearcam_pi_lib PRIVATE pio)




        #include_directories(/home/pi/git/utils/piolib/include/)

    endif()

    #target_link_libraries(linearcam_pi PRIVATE wiringPi)
    target_link_libraries(linearcam_pi_lib PRIVATE wiringPi)
endif()
target_link_libraries(linearcam_pi PRIVATE linearcam_pi_lib)

include(GNUInstallDirs)
install(TARGETS linearcam_pi
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
