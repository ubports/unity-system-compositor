# Copyright © 2015 Canonical Ltd.
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
# Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>

include_directories(
 ${CMAKE_SOURCE_DIR}
 ${MIRSERVER_INCLUDE_DIRS}
 ${DBUS_INCLUDE_DIRS}
)

add_executable(
  usc_integration_tests

  dbus_bus.cpp
  dbus_client.cpp
  test_dbus_event_loop.cpp
)

target_link_libraries(
   usc_integration_tests

   usc
   ${GTEST_BOTH_LIBRARIES}
   ${GMOCK_LIBRARY}
   ${GMOCK_MAIN_LIBRARY}
)

add_test(usc_integration_tests ${EXECUTABLE_OUTPUT_PATH}/usc_integration_tests)

add_dependencies(usc_integration_tests GMock)
