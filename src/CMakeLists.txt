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

set(USC_SRCS
  asio_dm_connection.cpp
  dbus_connection_handle.cpp
  dbus_event_loop.cpp
  dbus_message_handle.cpp
  external_spinner.cpp  
  mir_screen.cpp
  mir_input_configuration.cpp
  screen_event_handler.cpp
  session_switcher.cpp
  steady_clock.cpp
  system_compositor.cpp
  thread_name.cpp
  dbus_connection_thread.cpp
  validate_video_driver.cpp
  unity_input_service.cpp
  unity_input_service_introspection.h
  unity_display_service.cpp
  unity_display_service_introspection.h
  unity_power_button_event_sink.cpp
  unity_user_activity_event_sink.cpp
  window_manager.cpp
)

# Generate unity_display_service_introspection.h from the introspection XML file
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/unity_display_service_introspection.h
  COMMAND sh generate_header_with_string_from_file.sh ${CMAKE_CURRENT_BINARY_DIR}/unity_display_service_introspection.h unity_display_service_introspection com.canonical.Unity.Display.xml
  DEPENDS com.canonical.Unity.Display.xml generate_header_with_string_from_file.sh
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  VERBATIM
)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/unity_input_service_introspection.h
  COMMAND sh generate_header_with_string_from_file.sh ${CMAKE_CURRENT_BINARY_DIR}/unity_input_service_introspection.h unity_input_service_introspection com.canonical.Unity.Input.xml
  DEPENDS com.canonical.Unity.Input.xml generate_header_with_string_from_file.sh
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  VERBATIM
)

# Compile system compositor
add_library(
  usc STATIC
  ${USC_SRCS}
)

add_executable(
  unity-system-compositor
  main.cpp
)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${Boost_INCLUDE_DIRS}
  ${GLESv2_INCLUDE_DIRS}
  ${MIRAL_INCLUDE_DIRS}
  ${MIRSERVER_INCLUDE_DIRS}
  ${MIRCLIENT_INCLUDE_DIRS}
  ${DBUS_INCLUDE_DIRS}
)
add_definitions(
  -DDEFAULT_SPINNER="${CMAKE_INSTALL_FULL_BINDIR}/unity-system-compositor-spinner"
)

link_directories(${MIRSERVER_LIBRARY_DIRS})

target_link_libraries(usc
  ${MIRAL_STATIC_LDFLAGS}
  ${MIRSERVER_STATIC_LDFLAGS}  # Meaning usc is STATIC, _not_ libmirserver.
  pthread
  ${Boost_LIBRARIES}
  ${GLESv2_LIBRARIES}
  ${DBUS_LIBRARIES}
)

target_link_libraries(unity-system-compositor
  usc
)

# Install into bin directory
install(TARGETS unity-system-compositor
  RUNTIME DESTINATION sbin
)

# Install data files
install(FILES 
    com.canonical.Unity.conf
  DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/dbus-1/system.d
)
install(FILES
    com.canonical.Unity.Display.xml
    com.canonical.Unity.Input.xml
    com.canonical.Unity.PowerButton.xml
    com.canonical.Unity.UserActivity.xml
  DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/interfaces
)

