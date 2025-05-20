cmake_minimum_required(VERSION 3.11)
project(LinearScanCamApp)


#set(DUMMY ON)
#add_compile_definitions(DUMMY)
if(${DUMMY})
    add_compile_definitions(DUMMY)
else()
    set(PIOLIB ON)
    message("Non dummy build")
    add_compile_definitions(PIOLIB)
    #include_directories(epson_ccd_pio)
    #add_subdirectory(epson_ccd_pio)

endif()
include_directories(LinearScanCam)
include_directories(linearcam_pi)

add_subdirectory(linearcam_pi)

add_subdirectory(LinearScanCam)


#target_link_libraries(LinearScanCam PUBLIC linearcam_pi_lib)
