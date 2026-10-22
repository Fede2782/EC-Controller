cmake_minimum_required(VERSION 3.20)

# Project identity and the global C++ language level are defined once here so
# every subdirectory inherits the same clean, predictable build behavior.
project(msicontroller VERSION 1.0.1 LANGUAGES CXX)

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
set(MSI_CONTROLLER_SYSTEMD_UNIT_DIR "${CMAKE_INSTALL_PREFIX}/lib/systemd/system")

# The systemd unit needs the real install-time path to the backend executable,
# so we configure it once from the source template.
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/org.fedelabs.eccontroller.service"
    "${CMAKE_CURRENT_BINARY_DIR}/org.fedelabs.eccontroller.service"
    @ONLY
)

# Install the runtime configuration files from the project root. They remain
# editable at the source level while still landing in the standard Linux
# configuration directories selected by CMake.
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/org.fedelabs.EcController.conf"
    DESTINATION "${MSI_CONTROLLER_DBUS_SYSTEM_DIR}"
)

install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/org.fedelabs.EcController.policy"
    DESTINATION "${MSI_CONTROLLER_POLKIT_ACTIONS_DIR}"
)

# Polkit rule files are commonly installed with a numeric prefix so their
# ordering stays deterministic.
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/org.fedelabs.EcController.rules"
    DESTINATION "${MSI_CONTROLLER_POLKIT_RULES_DIR}"
    RENAME "49-org.fedelabs.EcController.rules"
)

# The configured unit file is installed into the systemd unit directory chosen
# by GNUInstallDirs, so custom prefixes automatically work.
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/org.fedelabs.eccontroller.service"
    DESTINATION "${MSI_CONTROLLER_SYSTEMD_UNIT_DIR}"
)

# Start the backend service automatically: enable it for every boot and
# start it right away, so the application works immediately after
# installing. Only on a direct install - when installing into a staging
# area (DESTDIR) for a package, enabling is the package manager's job.
# Failure (not root, no systemd) is reported but doesn't stop the install.
install(CODE [[
    if("$ENV{DESTDIR}" STREQUAL "")
        find_program(MSI_CONTROLLER_SYSTEMCTL systemctl)
        if(MSI_CONTROLLER_SYSTEMCTL)
            execute_process(COMMAND "${MSI_CONTROLLER_SYSTEMCTL}" --no-ask-password daemon-reload)
            execute_process(
                COMMAND "${MSI_CONTROLLER_SYSTEMCTL}" --no-ask-password enable --now org.fedelabs.eccontroller.service
                RESULT_VARIABLE MSI_CONTROLLER_ENABLE_RESULT)
            if(MSI_CONTROLLER_ENABLE_RESULT EQUAL 0)
                message(STATUS "Enabled and started org.fedelabs.eccontroller.service")
            else()
                message(WARNING "Couldn't enable org.fedelabs.eccontroller.service. "
                                "Enable it with: sudo systemctl enable --now org.fedelabs.eccontroller.service")
            endif()
        endif()
    endif()
]])

# Application and tray icons: the whole set icons/gen.sh generates. The
# generated tree is committed, so installing never needs an SVG renderer -
# only regenerating it does.
#
# - hicolor: the fallback theme every desktop searches.
# - breeze / breeze-dark: KDE's themes, with the tray glyph in the colour
#   that suits each (dark on Breeze, light on Breeze Dark). Only icon files
#   are added; each theme's index.theme comes from breeze-icons, and icon
#   lookup merges a theme's directories across every data dir, so this
#   also works under a /usr/local prefix.
# - pixmaps: the legacy location some launchers still read.
set(MSI_CONTROLLER_GENERATED_ICONS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/icons/icons")

foreach(MSI_CONTROLLER_ICON_THEME hicolor breeze breeze-dark)
    install(DIRECTORY
        "${MSI_CONTROLLER_GENERATED_ICONS_DIR}/${MSI_CONTROLLER_ICON_THEME}"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons"
    )
endforeach()

install(DIRECTORY
    "${MSI_CONTROLLER_GENERATED_ICONS_DIR}/pixmaps/"
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/pixmaps"
)

# An existing GTK icon cache that's now older than the icons it indexes can
# hide them from applications that trust it, so refresh any cache this
# install just made stale. Only caches that already exist are touched - one
# is never created - and nothing happens when installing into a staging
# area (DESTDIR): there the package manager's icon-cache triggers are
# responsible, and a cache written into the package would clash with the
# ones breeze-icons and hicolor-icon-theme already own.
#
# The data dir is fixed at configure time; the prefix is read at install
# time, so "cmake --install --prefix" is honoured. An absolute data dir is
# used as given.
install(CODE "set(MSI_CONTROLLER_ICONS_DATA_DIR \"${CMAKE_INSTALL_DATAROOTDIR}/icons\")")
install(CODE [[
    if("$ENV{DESTDIR}" STREQUAL "")
        find_program(MSI_CONTROLLER_ICON_CACHE_TOOL NAMES gtk-update-icon-cache gtk4-update-icon-cache)
        if(MSI_CONTROLLER_ICON_CACHE_TOOL)
            foreach(theme hicolor breeze breeze-dark)
                if(IS_ABSOLUTE "${MSI_CONTROLLER_ICONS_DATA_DIR}")
                    set(theme_dir "${MSI_CONTROLLER_ICONS_DATA_DIR}/${theme}")
                else()
                    set(theme_dir "${CMAKE_INSTALL_PREFIX}/${MSI_CONTROLLER_ICONS_DATA_DIR}/${theme}")
                endif()
                if(EXISTS "${theme_dir}/icon-theme.cache")
                    message(STATUS "Refreshing icon cache: ${theme_dir}")
                    execute_process(COMMAND "${MSI_CONTROLLER_ICON_CACHE_TOOL}"
                        --quiet --force --ignore-theme-index "${theme_dir}")
                endif()
            endforeach()
        endif()
    endif()
]])

# The desktop entry gives the application its menu entry and, on Wayland,
# its window identity (QGuiApplication::desktopFileName in main.cpp must
# keep matching this file's name).
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/org.fedelabs.EcController.desktop"
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications"
)

# Validate the desktop entry when desktop-file-validate is available.
find_program(MSI_CONTROLLER_DESKTOP_FILE_VALIDATE desktop-file-validate)
if(MSI_CONTROLLER_DESKTOP_FILE_VALIDATE)
    add_custom_target(validate-desktop ALL
        COMMAND "${MSI_CONTROLLER_DESKTOP_FILE_VALIDATE}" "${CMAKE_CURRENT_SOURCE_DIR}/org.fedelabs.EcController.desktop"
        COMMENT "Validating org.fedelabs.EcController.desktop"
    )
endif()

install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/org.fedelabs.EcController.metainfo.xml"
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/metainfo"
)

# Start the application at every user's login, hidden in the tray. Users
# opt out from Settings > Start at Login (or KDE's Autostart settings),
# which writes a Hidden=true override of this same file name into their
# own ~/.config/autostart. SYSCONFDIR is /etc for a /usr prefix.
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/ec-controller-autostart.desktop"
    DESTINATION "${CMAKE_INSTALL_FULL_SYSCONFDIR}/xdg/autostart"
)

# -----------------------------------------------------------------------------
# Packaging: .deb and .rpm through CPack (built in CI by
# .github/workflows/packages.yml)
# -----------------------------------------------------------------------------
# Configure with -DCMAKE_INSTALL_PREFIX=/usr when packaging: the systemd
# unit's ExecStart path and the absolute destinations above are fixed at
# configure time.
set(CPACK_PACKAGE_NAME "ec-controller")
set(CPACK_PACKAGE_VENDOR "FedeLabs")
set(CPACK_PACKAGE_CONTACT "Fede2782 <78815152+Fede2782@users.noreply.github.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Control MSI laptop hardware through the msi-ec driver")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/Fede2782/ec-controller")
set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Some destinations are absolute (the systemd unit, the autostart entry).
# With DESTDIR, CPack stages those inside the package like everything else
# instead of installing them onto the build machine. It also makes the
# install-time systemctl and icon-cache steps skip themselves: in a
# package, the maintainer scripts below take their place.
set(CPACK_SET_DESTDIR ON)

# Every push builds the same project version, so CI passes its run number
# as the package release to keep successive builds in upgrade order.
set(MSI_CONTROLLER_PACKAGE_RELEASE "1" CACHE STRING "Package release number (the N in <version>-N)")

# Runtime dependencies. Shared libraries (Qt, KF6, polkit-qt) are detected
# automatically from the binaries - dpkg-shlibdeps for the .deb, rpmbuild's
# own scan for the .rpm - and merged with what's listed here, which is
# what the code needs that isn't a library it links:
#
# Required:
#   polkit daemon  - the service authorizes every change through it
#   D-Bus          - the GUI and the service talk over the system bus
#
# Recommended (installed by default, but each feature switches itself off
# when its piece is missing):
#   Qt SVG plugin  - the icons are SVG
#   Qt Wayland     - native Wayland windows instead of XWayland
#   UPower         - keyboard backlight brightness
#   libkscreen     - kscreen-doctor, for the display flip shortcut
#
# Package names differ between the two families, hence two lists each.
set(CPACK_DEBIAN_PACKAGE_DEPENDS "polkitd" "dbus")
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "qt6-svg-plugins, qt6-wayland, upower, libkscreen-bin")
set(CPACK_RPM_PACKAGE_REQUIRES "polkit, dbus")
# CPack has no variable for the RPM Recommends tag; this hook inserts its
# text into the spec preamble, where a tag line is valid. (Suggests, which
# CPack does support, is not installed by default.)
set(CPACK_RPM_SPEC_MORE_DEFINE "Recommends: qt6-qtsvg, qt6-qtwayland, upower, libkscreen\n%{?systemd_requires}")

# Enable and start the backend service on install, stop it on removal.
# Debian packages use shell maintainer scripts; RPM uses systemd macros.
set(MSI_CONTROLLER_POSTINST "${CMAKE_CURRENT_SOURCE_DIR}/packaging/postinst")
set(MSI_CONTROLLER_PRERM "${CMAKE_CURRENT_SOURCE_DIR}/packaging/prerm")

set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_RELEASE "${MSI_CONTROLLER_PACKAGE_RELEASE}")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${MSI_CONTROLLER_POSTINST};${MSI_CONTROLLER_PRERM}")
set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION TRUE)

set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_RELEASE "${MSI_CONTROLLER_PACKAGE_RELEASE}")
set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0-only")
set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/packaging/postinst.rpm")
set(CPACK_RPM_PRE_UNINSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/packaging/prerm.rpm")
set(CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/packaging/postun.rpm")

include(CPack)

# Show a short summary of discovered packages and configuration status.
feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
