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

# Compile system compositor
add_executable(
  unity-system-compositor
  main.cpp
  dbus_screen.cpp
  dbus_screen.h
  dm_connection.cpp
  dm_connection.h
  system_compositor.cpp
  system_compositor.h
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
