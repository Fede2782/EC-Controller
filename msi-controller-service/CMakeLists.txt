# Backend service sources and the generated D-Bus XML stay together here.
add_executable(msi-controller-service
    main.cpp
    msicontrollerbackend.h
    msicontrollerbackend.cpp
    msicontrollerbackend_settings.cpp
    msicontrollerbackend_utils.cpp
)

qt_generate_dbus_interface(
    msicontrollerbackend.h
    org.fedelabs.MsiControllerAPI.xml
    OPTIONS -M
)

# Qt generates the XML through a custom command, so this target gives the
# executable a clean dependency that guarantees the file exists first.
add_custom_target(generate_dbus_xml
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/org.fedelabs.MsiControllerAPI.xml"
)

add_dependencies(msi-controller-service generate_dbus_xml)

target_link_libraries(msi-controller-service PRIVATE
    Qt6::Core
    Qt6::DBus
    PolkitQt6-1::Core
    msi-controller-lib
)

install(TARGETS msi-controller-service
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
