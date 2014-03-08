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
  ${GLESv2_INCLUDE_DIRS}
  ${LIBRSVG_INCLUDE_DIRS}
  ${MIRCLIENT_INCLUDE_DIRS}
)

link_directories(${MIRCLIENT_LIBRARY_DIRS})

add_executable(spinner eglapp.c eglapp.h egltriangle.c)
target_link_libraries(spinner
  EGL
  mirclient
  rsvg-2
  ${GLESv2_LIBRARIES}
)
install(TARGETS spinner RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/${PACKAGE})
