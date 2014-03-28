# -*- Mode: CMake; indent-tabs-mode: nil; tab-width: 2 -*-
#
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

include_directories(
  ${CAIRO_INCLUDE_DIRS}
  ${GLIB_INCLUDE_DIRS}
  ${GLESv2_INCLUDE_DIRS}
  ${MIRCLIENT_INCLUDE_DIRS}
)
add_definitions(
  -DPACKAGE="${PACKAGE}"
  -DLOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}"
  -DPKGDATADIR="${CMAKE_INSTALL_FULL_DATAROOTDIR}/${PACKAGE}"
)

link_directories(${MIRCLIENT_LIBRARY_DIRS})

add_executable(unity-system-compositor-spinner eglapp.c eglapp.h eglspinner.c)
target_link_libraries(unity-system-compositor-spinner
  EGL
  ${CAIRO_LDFLAGS}
  ${GLIB_LDFLAGS}
  ${GLESv2_LIBRARIES}
  ${MIRCLIENT_LDFLAGS}
)
install(TARGETS unity-system-compositor-spinner RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES spinner.png DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PACKAGE})
