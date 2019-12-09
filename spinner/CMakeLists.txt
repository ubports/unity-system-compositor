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

function(png2header png header varname)
  add_custom_command(
    OUTPUT ${header}
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/png2header.py ${png} ${varname} > ${header}
    DEPENDS ${png} ${CMAKE_SOURCE_DIR}/tools/png2header.py
  )
endfunction()

png2header(
  ${CMAKE_CURRENT_SOURCE_DIR}/robot.png
  ${CMAKE_CURRENT_BINARY_DIR}/robot.h
  robot
)

png2header(
  ${CMAKE_CURRENT_SOURCE_DIR}/white-dot.png
  ${CMAKE_CURRENT_BINARY_DIR}/white_dot.h
  white_dot
)

png2header(
  ${CMAKE_CURRENT_SOURCE_DIR}/orange-dot.png
  ${CMAKE_CURRENT_BINARY_DIR}/orange_dot.h
  orange_dot
)

include_directories(
  ${GDKPIXBUF_INCLUDE_DIRS}
  ${GLIB_INCLUDE_DIRS}
  ${DEVICEINFO_INCLUDE_DIRS}
  ${GLESv2_INCLUDE_DIRS}
  ${MIRCLIENT_INCLUDE_DIRS}
  ${CMAKE_CURRENT_BINARY_DIR}
)

link_directories(${MIRCLIENT_LIBRARY_DIRS})

add_executable(unity-system-compositor-spinner
  mir/eglapp.cpp
  mir/eglapp.h
  mir/miregl.h
  mir/miregl.cpp

  wayland/eglapp.cpp
  wayland/eglapp.h
  wayland/waylandegl.h
  wayland/waylandegl.cpp

  eglspinner.cpp

  ${CMAKE_CURRENT_BINARY_DIR}/robot.h
  ${CMAKE_CURRENT_BINARY_DIR}/white_dot.h
  ${CMAKE_CURRENT_BINARY_DIR}/orange_dot.h
)

target_link_libraries(unity-system-compositor-spinner
  EGL
  ${GDKPIXBUF_LDFLAGS}
  ${GLIB_LDFLAGS}
  ${DEVICEINFO_LDFLAGS}
  ${GLESv2_LIBRARIES}
  ${MIRCLIENT_LDFLAGS}
  ${WAYLAND_EGL_LDFLAGS} ${WAYLAND_EGL_LIBRARIES}
)
install(TARGETS unity-system-compositor-spinner RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
