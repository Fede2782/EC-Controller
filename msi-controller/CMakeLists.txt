# SPDX-FileCopyrightText: 2026 Fede2782
# SPDX-License-Identifier: GPL-3.0-or-later

# GUI executable sources stay in one place, while the generated DBus interface
# from the service build tree is appended automatically.
add_executable(msi-controller)

# The program is installed as "ec-controller". The target and its source
# directory keep their original names.
set_target_properties(msi-controller PROPERTIES OUTPUT_NAME ec-controller)

set(MSI_CONTROLLER_GUI_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    backendclient.cpp
    backendclient.h
    hardwaremonitor.cpp
    hardwaremonitor.h
    fanspeedmonitor.cpp
    fanspeedmonitor.h
    kbdbacklightbrightnessclient.cpp
    kbdbacklightbrightnessclient.h
    kdepowermanagementwatcher.cpp
    kdepowermanagementwatcher.h
    featuregroupbox.cpp
    featuregroupbox.h
    topicboxarea.cpp
    topicboxarea.h
    trayicon.cpp
    trayicon.h
    appactivationservice.cpp
    appactivationservice.h
    displayrotation.cpp
    displayrotation.h
    autostart.cpp
    autostart.h
    fancurvechart.cpp
    fancurvechart.h
    fancurveeditor.cpp
    fancurveeditor.h
    fancurvedialog.cpp
    fancurvedialog.h
)

qt_add_dbus_interface(
    MSI_CONTROLLER_GUI_SOURCES
    "${CMAKE_CURRENT_BINARY_DIR}/../msi-controller-service/org.fedelabs.EcControllerAPI.xml"
    msicontrollerbackend_interface
)

target_sources(msi-controller PRIVATE ${MSI_CONTROLLER_GUI_SOURCES})

# Embedded copies of the application and tray icons, used only when the
# installed theme icons can't be found (running from the build tree before
# "make install"). They come from the same generated set the install rule
# in the top-level CMakeLists.txt ships, so the two can't drift apart.
# BASE strips the path so they appear as :/icons/apps/... and
# :/icons/status/...
set(MSI_CONTROLLER_ICON_ROOT "${PROJECT_SOURCE_DIR}/icons/icons/hicolor/scalable")
qt_add_resources(msi-controller "fallback_icons"
    PREFIX "/icons"
    BASE "${MSI_CONTROLLER_ICON_ROOT}"
    FILES
        "${MSI_CONTROLLER_ICON_ROOT}/apps/ec-controller.svg"
        "${MSI_CONTROLLER_ICON_ROOT}/status/ec-controller-tray.svg"
)

if(TARGET Qt6::lrelease)
    qt_add_translations(msi-controller
        TS_FILES
            translations/ec-controller_en.ts
            translations/ec-controller_it.ts
        RESOURCE_PREFIX
            "/i18n"
    )
endif()

target_link_libraries(msi-controller PRIVATE
    Qt6::Widgets
    Qt6::DBus
    KF6::CoreAddons
    KF6::GlobalAccel
    KF6::Notifications
    msi-controller-lib
)

# The generated DBus interface headers live in the build tree, so that binary
# directory must be visible to the compiler.
target_include_directories(msi-controller PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}"
)

# The GUI is built after the service target so the generated DBus XML exists
# before the interface code is produced.
add_dependencies(msi-controller msi-controller-service)

install(TARGETS msi-controller
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
