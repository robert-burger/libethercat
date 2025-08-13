# libethercatConfig.cmake - pkg-config wrapper for libethercat

find_package(PkgConfig REQUIRED)
pkg_check_modules(PC_LIBETHERCAT REQUIRED libethercat)

find_library(LIBETHERCAT_LIB NAMES ethercat)
if(NOT LIBETHERCAT_LIB)
  message(FATAL_ERROR "Could not find libethercat shared library")
endif()

# Legacy variable for older CMake usage
set(libethercat_LIBS ${PC_LIBETHERCAT_LIBRARIES} CACHE INTERNAL "Link flags for libethercat")

add_library(libethercat::libethercat SHARED IMPORTED)
set_target_properties(libethercat::libethercat PROPERTIES
    IMPORTED_LOCATION "${LIBETHERCAT_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${PC_LIBETHERCAT_INCLUDEDIR}"
)

separate_arguments(PC_LIBETHERCAT_LIBS UNIX_COMMAND "${PC_LIBETHERCAT_LIBRARIES}")

set(LINK_DIRS "")
set(LINK_LIBS "")

foreach(lib ${PC_LIBETHERCAT_LIBS})
  if(lib MATCHES "^-l(.+)$")
    list(APPEND LINK_LIBS "${CMAKE_MATCH_1}")
  elseif(lib MATCHES "^-L(.+)$")
    list(APPEND LINK_DIRS "${CMAKE_MATCH_1}")
  else()
    list(APPEND LINK_LIBS "${lib}")
  endif()
endforeach()

if(LINK_DIRS)
  target_link_directories(libethercat::libethercat INTERFACE ${LINK_DIRS})
endif()

target_link_libraries(libethercat::libethercat INTERFACE ${LINK_LIBS})
