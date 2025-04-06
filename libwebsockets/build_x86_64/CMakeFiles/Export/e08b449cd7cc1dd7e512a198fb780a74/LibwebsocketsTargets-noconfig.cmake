#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "websockets" for configuration ""
set_property(TARGET websockets APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(websockets PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "C"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libwebsockets.a"
  )

list(APPEND _cmake_import_check_targets websockets )
list(APPEND _cmake_import_check_files_for_websockets "${_IMPORT_PREFIX}/lib/libwebsockets.a" )

# Import target "websockets_shared" for configuration ""
set_property(TARGET websockets_shared APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(websockets_shared PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libwebsockets.19.dylib"
  IMPORTED_SONAME_NOCONFIG "/Users/mymac/v8ui/libwebsockets/build_x86_64/install/lib/libwebsockets.19.dylib"
  )

list(APPEND _cmake_import_check_targets websockets_shared )
list(APPEND _cmake_import_check_files_for_websockets_shared "${_IMPORT_PREFIX}/lib/libwebsockets.19.dylib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
