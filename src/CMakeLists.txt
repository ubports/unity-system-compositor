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
  main.cpp
  dbus_screen.cpp
  dbus_screen.h
  powerd_mediator.cpp
  dm_connection.cpp
  dm_connection.h
  system_compositor.cpp
  system_compositor.h
  screen_state_handler.cpp
  screen_state_handler.h
  powerkey_handler.cpp
  powerkey_handler.h
)

qt5_generate_dbus_interface(dbus_screen.h com.canonical.Unity.Screen.xml)
qt5_add_dbus_adaptor(USC_SRCS
                     ${CMAKE_CURRENT_BINARY_DIR}/com.canonical.Unity.Screen.xml
                     dbus_screen.h DBusScreen
                     dbus_screen_adaptor DBusScreenAdaptor)

# Compile system compositor
add_executable(
  unity-system-compositor
  ${USC_SRCS}
)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)

# Link against libmirserver
target_link_libraries(unity-system-compositor
  mirserver
  pthread
  ${Boost_LIBRARIES}
  ${GLESv2_LIBRARIES}
)
qt5_use_modules(unity-system-compositor Core DBus)

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
