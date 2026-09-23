# GUI executable sources stay in one place, while the generated DBus interface
# from the service build tree is appended automatically.
add_executable(msi-controller)

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
    featuregroupbox.cpp
    featuregroupbox.h
)

qt_add_dbus_interface(
    MSI_CONTROLLER_GUI_SOURCES
    "${CMAKE_CURRENT_BINARY_DIR}/../msi-controller-service/org.fedelabs.MsiControllerAPI.xml"
    msicontrollerbackend_interface
)

target_sources(msi-controller PRIVATE ${MSI_CONTROLLER_GUI_SOURCES})

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
