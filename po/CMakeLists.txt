cmake_minimum_required(VERSION 2.8.9)
find_package(Gettext REQUIRED)

set(POT_FILE ${PACKAGE}.pot)
set(MO_FILE ${PACKAGE}.mo)

file(GLOB PO_FILES *.po)
foreach(_file ${PO_FILES})
  get_filename_component(_lang ${_file} NAME_WE)
  set(_gmoFile ${CMAKE_CURRENT_BINARY_DIR}/${_lang}.gmo)
  add_custom_command(OUTPUT ${_gmoFile}
    COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} -o ${_gmoFile} ${_file}
    COMMAND install -D ${_gmoFile} ${CMAKE_CURRENT_BINARY_DIR}/locale/${_lang}/LC_MESSAGES/${MO_FILE}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    DEPENDS ${_file}
  )
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${_lang}.gmo DESTINATION ${CMAKE_INSTALL_PREFIX}/share/locale/${_lang}/LC_MESSAGES/ RENAME ${MO_FILE})
  _GETTEXT_GET_UNIQUE_TARGET_NAME(pofiles uniqueTargetName)
  add_custom_target(${uniqueTargetName} ALL DEPENDS ${_gmoFile})
endforeach(_file)

find_program(INTLTOOL_UPDATE intltool-update)
if(INTLTOOL_UPDATE)
add_custom_target(${POT_FILE}
  COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR} && ${INTLTOOL_UPDATE} --pot --gettext-package=${PACKAGE}
)
add_custom_target(pot_file DEPENDS ${POT_FILE})
endif()
