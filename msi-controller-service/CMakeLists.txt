# SPDX-FileCopyrightText: 2026 Fede2782
# SPDX-License-Identifier: GPL-3.0-or-later

# Backend service sources and the generated D-Bus XML stay together here.
add_executable(msi-controller-service
    main.cpp
    msicontrollerbackend.h
    msicontrollerbackend.cpp
    msicontrollerbackend_settings.cpp
    msicontrollerbackend_utils.cpp
)

# The program is installed as "ec-controller-service". The target and its
# source directory keep their original names.
set_target_properties(msi-controller-service PROPERTIES OUTPUT_NAME ec-controller-service)

qt_generate_dbus_interface(
    msicontrollerbackend.h
    org.fedelabs.EcControllerAPI.xml
    OPTIONS -M
)

# Qt generates the XML through a custom command, so this target gives the
# executable a clean dependency that guarantees the file exists first.
add_custom_target(generate_dbus_xml
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/org.fedelabs.EcControllerAPI.xml"
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
