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
    COMMAND python ${CMAKE_SOURCE_DIR}/tools/png2header.py ${png} ${varname} > ${header}
    DEPENDS ${png} ${CMAKE_SOURCE_DIR}/tools/png2header.py
  )
endfunction()

png2header(
  ${CMAKE_CURRENT_SOURCE_DIR}/wallpaper.png
  ${CMAKE_CURRENT_BINARY_DIR}/wallpaper.h
  wallpaper
)

png2header(
  ${CMAKE_CURRENT_SOURCE_DIR}/logo.png
  ${CMAKE_CURRENT_BINARY_DIR}/logo.h
  logo
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
  ${GLIB_INCLUDE_DIRS}
  ${ANDROIDPROPS_INCLUDE_DIRS}
  ${GLESv2_INCLUDE_DIRS}
  ${MIRCLIENT_INCLUDE_DIRS}
  ${CMAKE_CURRENT_BINARY_DIR}
)

if(ANDROIDPROPS_FOUND)
add_definitions(-DHAVE_PROPS)
endif()

link_directories(${MIRCLIENT_LIBRARY_DIRS})

add_executable(unity-system-compositor-spinner
  eglapp.cpp
  eglapp.h
  eglspinner.cpp
  miregl.h
  miregl.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/wallpaper.h
  ${CMAKE_CURRENT_BINARY_DIR}/logo.h
  ${CMAKE_CURRENT_BINARY_DIR}/white_dot.h
  ${CMAKE_CURRENT_BINARY_DIR}/orange_dot.h
)

target_link_libraries(unity-system-compositor-spinner
  EGL
  ${GLIB_LDFLAGS}
  ${ANDROIDPROPS_LDFLAGS}
  ${GLESv2_LIBRARIES}
  ${MIRCLIENT_LDFLAGS}
)
install(TARGETS unity-system-compositor-spinner RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
