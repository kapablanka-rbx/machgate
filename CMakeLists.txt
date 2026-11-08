cmake_minimum_required(VERSION 3.16)
project(machgate C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
    message(FATAL_ERROR "MachGate requires an ARM64 Linux target")
endif()

add_library(machgate_domain STATIC
    src/domain/dwarf_unwind.c
    src/domain/guest_process.c
    src/domain/guest_syscall.c
    src/domain/process_launch_context.c
    src/domain/provider_decision.c
)
target_include_directories(machgate_domain PUBLIC src)
target_compile_options(machgate_domain PRIVATE -Wall -Wextra -O2 -g)

add_library(machgate_application STATIC
    src/application/guest_session.c
)
target_include_directories(machgate_application PUBLIC src)
target_compile_options(machgate_application PRIVATE -Wall -Wextra -O2 -g)
target_link_libraries(machgate_application PUBLIC machgate_domain)

add_executable(machgate
    src/machgate.c
    src/adapters/dynamic_unwind.c
    src/shim/file_abi.c
    src/adapters/guest_execution.c
    src/adapters/guest_platform.c
    src/adapters/guest_libraries.c
    src/adapters/guest_symbol_lookup.c
    src/adapters/guest_rebase.c
    src/adapters/guest_thread_local.c
    src/adapters/guest_unwind.c
    src/loader.c
    src/adapters/startup_cache.c
    src/instruction_adaptation.c
    src/native_imports.c
    src/native_structor_adapter.c
    src/native_structor_adapter_entry.S
    src/native_provider_map.c
    src/guest_symbols.c
    src/guest_process_runtime.c
    src/guest_allocator_bridge.c
    src/trampoline.c
    src/guest_call.S
    src/shim/process_termination_compat.c
    src/shim/kqueue_compat.c
    src/adapters/linux_event_timer.c
    src/adapters/linux_socket_state.c
    src/adapters/linux_user_lock.c
    src/syscall/darwin_ulock.c
    src/vm_interpose_loader.c
    src/syscall/syscall_gate.c
    src/syscall/guest_vm_dispatch.c
    src/syscall/guest_vm_track.c
    src/syscall/darwin_socket.c
    src/syscall/darwin_xattr.c
    src/shim/darwin_errno.c
    src/syscall/syscall_range_000_099.c
    src/syscall/syscall_range_100_199.c
    src/syscall/syscall_range_200_399.c
    src/syscall/syscall_range_400_plus.c
    src/syscall/execve_reexec.c
)

target_include_directories(machgate PRIVATE src)
target_compile_definitions(machgate PRIVATE _GNU_SOURCE MACHGATE_GUEST_RUNNER)
target_compile_options(machgate PRIVATE -Wall -Wextra -O2 -g)
target_link_libraries(machgate PRIVATE machgate_application machgate_domain dl pthread)
target_link_options(machgate PRIVATE -rdynamic)

add_library(vm_interpose SHARED src/vm_interpose.c)
set_target_properties(vm_interpose PROPERTIES
    OUTPUT_NAME "machgate_vm_interpose"
    PREFIX "lib"
)
target_compile_options(vm_interpose PRIVATE -Wall -O2 -g)
target_compile_definitions(vm_interpose PRIVATE _GNU_SOURCE)
target_link_libraries(vm_interpose PRIVATE dl)

add_library(system_shim SHARED
    src/shim/allocation_ledger.c
    src/shim/archive_system_compat.c
    src/shim/archive_system_abi_entry.S
    src/shim/libsystem_shim.c
    src/shim/assert_compat.c
    src/shim/autorelease_compat.c
    src/shim/basic_runtime_compat.c
    src/shim/blocks_compat.c
    src/shim/core_foundation_compat.c
    src/shim/darwin_errno.c
    src/shim/dispatch_compat.c
    src/shim/file_abi.c
    src/shim/file_operations_compat.c
    src/shim/file_operations_abi_entry.S
    src/shim/filesystem_metadata_compat.c
    src/shim/guest_import_resolver.c
    src/shim/extended_attributes_compat.c
    src/syscall/darwin_xattr.c
    src/shim/guest_process_compat.c
    src/shim/guest_environment_compat.c
    src/shim/image_symbols_compat.c
    src/shim/guest_hardware_compat.c
    src/shim/initializer_context.c
    src/shim/interface_addresses_compat.c
    src/guest_process_runtime.c
    src/shim/host_facts.c
    src/shim/host_statistics_compat.c
    src/shim/kqueue_compat.c
    src/adapters/linux_event_timer.c
    src/adapters/linux_socket_state.c
    src/adapters/linux_user_lock.c
    src/syscall/darwin_ulock.c
    src/shim/locale_string_compat.c
    src/shim/mach_sync_compat.c
    src/shim/notify_compat.c
    src/shim/objc_compat.c
    src/shim/osatomic_compat.c
    src/shim/passwd_compat.c
    src/shim/popen_compat.c
    src/shim/pthread_attr_compat.c
    src/shim/pthread_sync_compat.c
    src/shim/process_termination_compat.c
    src/shim/resource_limit_compat.c
    src/shim/darwin_error_messages_compat.c
    src/shim/registered_memory_compat.c
    src/shim/vm_region_compat.c
    src/adapters/linux_memory_region.c
    src/shim/security_compat.c
    src/shim/socket_compat.c
    src/shim/sysctl_compat.c
    src/shim/task_policy_compat.c
    src/shim/thread_local_compat.c
    src/shim/thread_local_entry.S
    src/shim/time_compat.c
    src/shim/user_lock_compat.c
    src/shim/unwind_compat.c
    src/shim/variadic_abi_compat.c
    src/shim/variadic_abi_entry.S
    src/syscall/darwin_socket.c
    src/syscall/execve_reexec.c
)
target_link_libraries(system_shim PRIVATE m pthread dl)
target_include_directories(system_shim PRIVATE src)
target_compile_definitions(system_shim PRIVATE MACHGATE_SYSTEM_SHIM)
target_compile_options(system_shim PRIVATE
    -Wall -O2 -g -fno-omit-frame-pointer
)
target_link_options(system_shim PRIVATE
    "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/src/shim/libsystem_shim.ver"
    "-Wl,--wrap=snprintf")
set_target_properties(system_shim PROPERTIES
    PREFIX "lib" OUTPUT_NAME "system_shim")

install(TARGETS machgate RUNTIME DESTINATION bin)
install(TARGETS vm_interpose LIBRARY DESTINATION lib)
install(TARGETS system_shim LIBRARY DESTINATION lib)

option(MACHGATE_BUILD_APPLE_PROVIDERS "Build the separate Apple compatibility providers" ON)
if(MACHGATE_BUILD_APPLE_PROVIDERS)
    add_subdirectory(providers/foundation)
    add_subdirectory(providers/icu)
    add_subdirectory(providers/apple_compat)
endif()
