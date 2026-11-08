if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.16)
    project(machgate_icu_provider C ASM)
endif()

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR
   NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
    message(FATAL_ERROR "The ICU native provider requires ARM64 Linux")
endif()

find_package(ICU 74 REQUIRED COMPONENTS uc i18n)
if(NOT ICU_VERSION VERSION_LESS "75")
    message(FATAL_ERROR "The ICU native provider requires the reviewed ICU 74 ABI")
endif()

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/exports.def")
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/exports.def" ICU_EXPORT_DECLARATIONS)
set(ICU_VERSION_SCRIPT "MACHGATE_ICU_1 {\n    global:\n")
foreach(ICU_EXPORT_DECLARATION IN LISTS ICU_EXPORT_DECLARATIONS)
    string(REGEX REPLACE "^MACHGATE_ICU\\(([^,]+),.*$" "\\1" ICU_EXPORT_NAME "${ICU_EXPORT_DECLARATION}")
    string(APPEND ICU_VERSION_SCRIPT "        ${ICU_EXPORT_NAME};\n")
endforeach()
string(APPEND ICU_VERSION_SCRIPT "    local: *;\n};\n")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/exports.ver" "${ICU_VERSION_SCRIPT}")

add_library(machgate_icu SHARED entry.S abi_checks.c host_runtime.c)
target_compile_features(machgate_icu PRIVATE c_std_11)
target_compile_options(machgate_icu PRIVATE -Wall -Wextra -Werror -O2 -g)
target_include_directories(machgate_icu PRIVATE ${ICU_INCLUDE_DIRS})
target_link_libraries(machgate_icu PRIVATE dl)
target_link_options(machgate_icu PRIVATE
    "-Wl,--no-undefined"
    "-Wl,--version-script=${CMAKE_CURRENT_BINARY_DIR}/exports.ver")
set_target_properties(machgate_icu PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    BUILD_RPATH "$ORIGIN" INSTALL_RPATH "$ORIGIN")
install(TARGETS machgate_icu LIBRARY DESTINATION lib)
