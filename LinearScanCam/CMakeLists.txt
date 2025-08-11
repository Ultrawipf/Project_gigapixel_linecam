cmake_minimum_required(VERSION 3.16)

project(LinearScanCam VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        json.hpp
)

# Common functions


#add_library(linearcam_pi ../linearcam_pi/constants_pi.h
#  ../linearcam_pi/tmc2209_motor.cpp ../linearcam_pi/tmc2209_motor.h)



if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

    qt_add_executable(LinearScanCam
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        cameracontrolqt.h cameracontrolqt.cpp
        resources.qrc
        libcamvidqt.h libcamvidqt.cpp
        confstore.h confstore.cpp
        pcfioqt.h pcfioqt.cpp
        imageviewqt.h imageviewqt.cpp


    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET LinearScanCam APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(LinearScanCam SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(LinearScanCam
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_include_directories(LinearScanCam PRIVATE "${EXTRA_SOURCES_INC}")

target_link_libraries(LinearScanCam PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
#add_dependencies(LinearScanCam "../linearcam_pi")
target_link_libraries(LinearScanCam PUBLIC linearcam_pi_lib)
target_link_libraries(LinearScanCam PRIVATE Qt6::Core Qt6::Widgets)


# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.LinearScanCam)
endif()
set_target_properties(LinearScanCam PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS LinearScanCam
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(LinearScanCam)
endif()

