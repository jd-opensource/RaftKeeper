add_executable (var_uint var_uint.cpp)
target_link_libraries (var_uint PRIVATE rk_common_io)

add_executable (parse_int_perf parse_int_perf.cpp)
target_link_libraries (parse_int_perf PRIVATE rk_common_io)

add_executable (parse_int_perf2 parse_int_perf2.cpp)
target_link_libraries (parse_int_perf2 PRIVATE rk_common_io)

add_executable (read_write_int read_write_int.cpp)
target_link_libraries (read_write_int PRIVATE rk_common_io)

add_executable (o_direct_and_dirty_pages o_direct_and_dirty_pages.cpp)
target_link_libraries (o_direct_and_dirty_pages PRIVATE rk_common_io)

add_executable (io_operators io_operators.cpp)
target_link_libraries (io_operators PRIVATE rk_common_io)

add_executable (write_int write_int.cpp)
target_link_libraries (write_int PRIVATE rk_common_io)

add_executable (limit_read_buffer limit_read_buffer.cpp)
target_link_libraries (limit_read_buffer PRIVATE rk_common_io)

add_executable (limit_read_buffer2 limit_read_buffer2.cpp)
target_link_libraries (limit_read_buffer2 PRIVATE rk_common_io)
