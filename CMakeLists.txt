cmake_minimum_required(VERSION 3.11)
project(LinearScanCamApp)


#set(DUMMY ON)
#add_compile_definitions(DUMMY)
if(${DUMMY})
    add_compile_definitions(DUMMY)
endif()
add_subdirectory(linearcam_pi)
#target_link_libraries(LinearScanCamApp PUBLIC linearcam_pi_lib)
add_subdirectory(LinearScanCam)
include_directories(linearcam_pi)

