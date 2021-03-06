# Copyright © 2014 Canonical Ltd.
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
)

add_executable(
  usc_unit_tests

  test_session_switcher.cpp
  test_screen_event_handler.cpp
  test_mir_screen.cpp
  test_mir_input_configuration.cpp

  advanceable_timer.cpp
)

target_link_libraries(
   usc_unit_tests

   usc
   ${GTEST_BOTH_LIBRARIES}
   ${GMOCK_LIBRARIES}
)

add_test(usc_unit_tests ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/usc_unit_tests)

add_dependencies(usc_unit_tests GMock)
