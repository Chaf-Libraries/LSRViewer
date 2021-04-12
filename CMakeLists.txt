cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(LSRViewer VERSION 1.0.1)
message(STATUS "[Project] ${PROJECT_NAME} ${PROJECT_VERSION}")

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(Init)
Init_Project()

add_subdirectory(extern)
add_subdirectory(source)

if (MSVC)
	 # Set the startup project .
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT LSRViewer)
endif (MSVC)