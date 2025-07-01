cmake_minimum_required(VERSION 3.10...3.27)

include(GNUInstallDirs)
# include(/home/pi/git/utils/piolib/)
set(PIOPATH /home/pi/git/utils/piolib/)
link_directories(${PIOPATH})

include_directories(/home/pi/git/utils/piolib/include/)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/pio_ccd.h
  COMMAND pioasm pio_ccd.pio pio_ccd.h
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/pio_ccd.pio
)

#set project name
project(pio_ccd_test)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

if (CMAKE_COMPILER_IS_GNUCC)
   add_definitions (-ffunction-sections)
endif ()

add_definitions (-DPIOLIB)

# common ccd files
file(GLOB CCD_FILES "${CMAKE_CURRENT_LIST_DIR}/../epson_ccd_pio/*.c")
#add_executable(pio_ccd ${CCD_FILES})

add_library (pio OBJECT ${PIOPATH}/piolib.c ${PIOPATH}/pio_rp1.c)
add_executable(pio_ccd pio_ccd.c ${CMAKE_CURRENT_BINARY_DIR}/pio_ccd.h ccd_constants.h)
target_include_directories(pio_ccd PRIVATE)
target_link_libraries(pio_ccd pio)
install(TARGETS pio_ccd RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
