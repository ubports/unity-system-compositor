# Copyright © 2013 Canonical Ltd.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Authored by: Robert Ancell <robert.ancell@canonical.com>

project(UnitySystemCompositor)
set(PACKAGE "unity-system-compositor")
set(USC_VERSION_MAJOR 0)
set(USC_VERSION_MINOR 0)
set(USC_VERSION_PATCH 4)
set(USC_VERSION "${USC_VERSION_MAJOR}.${USC_VERSION_MINOR}.${USC_VERSION_PATCH}")

cmake_minimum_required(VERSION 2.8)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(GNUInstallDirs)

# Set location of outputs
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)

add_definitions(-DUSC_VERSION="${USC_VERSION}")

set(CMAKE_AUTOMOC ON)

# Use C++11 and warnings
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")

find_package(PkgConfig)
pkg_check_modules(ANDROIDPROPS libandroid-properties)
pkg_check_modules(CAIRO REQUIRED cairo)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(MIRCLIENT REQUIRED mirclient)
pkg_check_modules(MIRSERVER REQUIRED mirserver)
pkg_check_modules(DBUS REQUIRED dbus-1)

find_package(Boost 1.48.0 COMPONENTS system REQUIRED)
find_package(GLESv2 REQUIRED)
find_package(Qt5Core)
find_package(Qt5DBus)

add_subdirectory(spinner/)
add_subdirectory(src/)

enable_testing()

option(MIR_ENABLE_TESTS "Build tests" ON)

if (MIR_ENABLE_TESTS)
  find_package(Gtest REQUIRED)
  include_directories(${GMOCK_INCLUDE_DIR} ${GTEST_INCLUDE_DIR})
  add_subdirectory(tests/)
endif ()
