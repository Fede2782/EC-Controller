cmake_minimum_required(VERSION 3.20)

# Project identity and the global C++ language level are defined once here so
# every subdirectory inherits the same clean, predictable build behavior.
project(msicontroller VERSION 0.3 LANGUAGES CXX)

# Standard GNU-style install directories keep the project portable across Linux
# distributions and make it easy for users to override the install layout with
# the normal CMake cache variables such as CMAKE_INSTALL_PREFIX and
# CMAKE_INSTALL_BINDIR.
include(GNUInstallDirs)

# FeatureSummary gives a compact report at the end of configuration.
include(FeatureSummary)

# Qt's automatic code generation is enabled globally because all subprojects
# use Qt UI, MOC, and resource features.
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Keep the codebase on modern, portable C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Centralize minimum dependency versions so the whole tree uses the same
# requirements.
set(MSI_CONTROLLER_QT_MIN_VERSION 6.10.0)
set(MSI_CONTROLLER_KF_MIN_VERSION 6.6.0)

# ECM is still needed for the KDE CMake helpers used by this project.
find_package(ECM ${MSI_CONTROLLER_KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

# Required runtime and GUI dependencies are discovered once at the top level.
find_package(Qt6 ${MSI_CONTROLLER_QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Core
    Widgets
    DBus
)

find_package(KF6 ${MSI_CONTROLLER_KF_MIN_VERSION} REQUIRED COMPONENTS
    CoreAddons
    GlobalAccel
    Notifications
)

find_package(PolkitQt6-1 REQUIRED)

# Build the shared library first so both executables can reuse the same code.
add_subdirectory(msi-controller-lib)
add_subdirectory(msi-controller-service)
add_subdirectory(msi-controller)

# Keep install destinations standard and easy to override with the usual CMake
# variables. These values expand to distro-friendly paths such as
# <prefix>/share and <prefix>/lib.
set(MSI_CONTROLLER_DBUS_SYSTEM_DIR "${CMAKE_INSTALL_DATAROOTDIR}/dbus-1/system.d")
set(MSI_CONTROLLER_POLKIT_ACTIONS_DIR "${CMAKE_INSTALL_DATAROOTDIR}/polkit-1/actions")
set(MSI_CONTROLLER_POLKIT_RULES_DIR "${CMAKE_INSTALL_DATAROOTDIR}/polkit-1/rules.d")
set(MSI_CONTROLLER_SYSTEMD_UNIT_DIR "${CMAKE_INSTALL_DATAROOTDIR}/systemd/system")

# The systemd unit needs the real install-time path to the backend executable,
# so we configure it once from the source template.
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/org.fedelabs.msicontroller.service"
    "${CMAKE_CURRENT_BINARY_DIR}/org.fedelabs.msicontroller.service"
    @ONLY
)

# Install the runtime configuration files from the project root. They remain
# editable at the source level while still landing in the standard Linux
# configuration directories selected by CMake.
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/org.fedelabs.MsiController.conf"
    DESTINATION "${MSI_CONTROLLER_DBUS_SYSTEM_DIR}"
)

install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/org.fedelabs.MsiController.policy"
    DESTINATION "${MSI_CONTROLLER_POLKIT_ACTIONS_DIR}"
)

# Polkit rule files are commonly installed with a numeric prefix so their
# ordering stays deterministic.
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/org.fedelabs.MsiController.rules"
    DESTINATION "${MSI_CONTROLLER_POLKIT_RULES_DIR}"
    RENAME "49-org.fedelabs.MsiController.rules"
)

# The configured unit file is installed into the systemd unit directory chosen
# by GNUInstallDirs, so custom prefixes automatically work.
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/org.fedelabs.msicontroller.service"
    DESTINATION "${MSI_CONTROLLER_SYSTEMD_UNIT_DIR}"
)

# Show a short summary of discovered packages and configuration status.
feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
