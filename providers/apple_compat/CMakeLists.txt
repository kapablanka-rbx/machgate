add_library(machgate_apple_compat SHARED
    fsevents.c
    unavailable_services.c
    unavailable_mach.c
    provider_resolution.c
)
target_include_directories(machgate_apple_compat PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_compile_options(machgate_apple_compat PRIVATE -Wall -Wextra -O2 -g)
target_link_libraries(machgate_apple_compat PRIVATE
    "-Wl,--no-as-needed" machgate_foundation machgate_icu system_shim "-Wl,--as-needed" pthread)
target_link_options(machgate_apple_compat PRIVATE
    "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/exports.ver")
set_target_properties(machgate_apple_compat PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    BUILD_RPATH "$ORIGIN" INSTALL_RPATH "$ORIGIN")
install(TARGETS machgate_apple_compat LIBRARY DESTINATION lib)
