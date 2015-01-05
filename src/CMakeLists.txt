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
  dbus_screen.cpp
  external_spinner.cpp
  powerd_mediator.cpp
  powerkey_handler.cpp
  screen_state_handler.cpp
  server.cpp
  session_coordinator.cpp
  session_switcher.cpp
  surface_coordinator.cpp
  system_compositor.cpp
  worker_thread.cpp
)

qt5_generate_dbus_interface(dbus_screen.h com.canonical.Unity.Screen.xml)
qt5_add_dbus_adaptor(USC_SRCS
                     ${CMAKE_CURRENT_BINARY_DIR}/com.canonical.Unity.Screen.xml
                     dbus_screen.h DBusScreen
                     dbus_screen_adaptor DBusScreenAdaptor)

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
  ${MIRSERVER_INCLUDE_DIRS}
)
add_definitions(
  -DDEFAULT_SPINNER="${CMAKE_INSTALL_FULL_BINDIR}/unity-system-compositor-spinner"
)

qt5_use_modules(usc Core DBus)

link_directories(${MIRSERVER_LIBRARY_DIRS})

target_link_libraries(usc
  ${MIRSERVER_STATIC_LDFLAGS}  # Meaning usc is STATIC, _not_ libmirserver.
  pthread
  ${Boost_LIBRARIES}
  ${GLESv2_LIBRARIES}
)

target_link_libraries(unity-system-compositor
  usc
)

# Install into bin directory
install(TARGETS unity-system-compositor
  RUNTIME DESTINATION sbin
)

# Install data files
install(FILES com.canonical.Unity.Screen.conf
  DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/dbus-1/system.d
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/com.canonical.Unity.Screen.xml
  DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/interfaces
)
