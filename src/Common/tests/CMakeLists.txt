add_executable (simple_cache simple_cache.cpp)
target_link_libraries (simple_cache PRIVATE common)

add_executable (pod_array pod_array.cpp)
target_link_libraries (pod_array PRIVATE rk_common_io)

add_executable (thread_creation_latency thread_creation_latency.cpp)
target_link_libraries (thread_creation_latency PRIVATE rk_common_io)

add_executable (stopwatch stopwatch.cpp)
target_link_libraries (stopwatch PRIVATE rk_common_io)

add_executable (symbol_index symbol_index.cpp)
target_link_libraries (symbol_index PRIVATE rk_common_io)

add_executable (chaos_sanitizer chaos_sanitizer.cpp)
target_link_libraries (chaos_sanitizer PRIVATE rk_common_io)

if (OS_LINUX)
    add_executable (memory_statistics_os_perf memory_statistics_os_perf.cpp)
    target_link_libraries (memory_statistics_os_perf PRIVATE rk_common_io)
endif()

add_executable (shell_command_inout shell_command_inout.cpp)
target_link_libraries (shell_command_inout PRIVATE rk_common_io)
