#pragma once 

extern "C" {
  void write_vtable(const char* bolted_binary_path, const char* vtable_output_file_path);
  void write_functions(const char* bolted_binary_path, const char* vtable_output_file_path, long* function_addrs, long num_addrs);
}
