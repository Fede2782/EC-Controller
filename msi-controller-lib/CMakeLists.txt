# SPDX-FileCopyrightText: 2026 Fede2782
# SPDX-License-Identifier: GPL-3.0-or-later

# Shared library used by both the GUI and the backend service.
add_library(msi-controller-lib STATIC
    msicontrollerlib.h
    msicontrollerlib_attributes.cpp
    msicontrollerlib_modes.cpp
    msicontrollerlib_settings.cpp
    msicontrollerlib_io.cpp
    msicontrollerlib_fancurve.cpp
)

target_link_libraries(msi-controller-lib PUBLIC Qt6::Core)

target_include_directories(msi-controller-lib PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
