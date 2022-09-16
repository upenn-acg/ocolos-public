#include "utils.hpp"

using namespace std;

/*
 * for each unmoved functions, check any call instruction 
 * within that function and if the call's target is moved,
 * patch the call sites by changing the target.
 */
void extract_call_sites(FILE* pFile, unordered_map<long, func_info>moved_func, unordered_map<long, func_info> func_in_call_stack, const ocolos_env* ocolos_environ);



// in libelf_extract.a
extern "C" {
  /*
   * Invoke Rust code to extract functions from the given 
   * binary. The starting addresses of functions to be 
   * extracted are passed by an array of long integers.
   */
   void write_functions(const char* bolted_binary_path, const char* vtable_output_file_path, long* function_addrs, long num_addrs);



  /*
   * Invoke Rust code to extract v-tables from the given
   * binary. The arguments are the path of the binary that
   * will have data extracted, and the path to store the 
   * extracted data.
   */ 
   void write_vtable(const char*, const char*);
}
