add_library(machgate_foundation SHARED
    objc_runtime.c
    exception_runtime.c
    objc_entry.S
    archive_entry_compat.c
    archive_forwarder.S
    image_registration.c
    string_value.c
    string_format.c
    url_path.c
    archive/objects.c
    archive/guest_classes.c
    archive/runtime_api.c
)
target_compile_definitions(machgate_foundation PRIVATE _GNU_SOURCE)
target_include_directories(machgate_foundation PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_compile_options(machgate_foundation PRIVATE -Wall -Wextra -O2 -g -ffixed-x18)
target_link_libraries(machgate_foundation PRIVATE system_shim dl pthread)
target_link_options(machgate_foundation PRIVATE
    "-Wl,-Bsymbolic"
    "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/exports.ver")
set_target_properties(machgate_foundation PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN")
install(TARGETS machgate_foundation LIBRARY DESTINATION lib)
