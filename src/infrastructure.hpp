#include "utils.hpp"

using namespace std;


/* 
 * functions in infrastructure.h perform 
 * (1) create target process and communicate with target process to 
 *     get starting address of `replace_function()`
 * (2) create perf, perf2bolt, llvm-bolt to generate the optimized 
 *     binary.
 * (3) prepare for the machine code to be inserted to the target 
 *     process. (extract machine code and then change the target
 *     of call instructions)
 */
void initialize_benchmark(const ocolos_env*);
void run_benchmark(const ocolos_env*);




/*
 * tracer to create the target process to 
 * be optimized. (e.g. MySQL server process)
 * The LD_PRELOAD library is added as the 
 * environment variable
 */
void create_target_server_process(const ocolos_env*);




/*
 * create tcp socket. The tcp socket is created 
 * to accept the message fromthe ld_preload library 
 * to get the starting address of `replace_function()`, 
 * which does the machine code insertion work.
 */
void create_tcp_socket(int listen_fd, struct sockaddr_in & servaddr);




/*
 * accept message from target process then convert the 
 * message into a pointer that represent the address of 
 * the `replace_function()` function in the ld_preload 
 * lib code.
 */
void* get_lib_addr(int listen_fd);




void send_data_path(const ocolos_env*);




/*
 * Create a new process that runs Linux perf. Perf will 
 * attach to the server process and collect profiles 
 * during server process's running.
 */
void run_perf_record(int target_pid, const ocolos_env*);




/*
 * Create a new process that runs BOLT's perf2bolt.
 * Perf2bolt will create a BOLT readable profile data
 * by taking the perf's output profiles and the original
 * server binary.
 */
void run_perf2bolt(const ocolos_env*);




/*
 * Create a new process that runs BOLT's llvm-bolt.
 * Llvm-bolt will create a BOLTed binary by taking the 
 * perf2bolt's output and original server binary.
 */
unordered_map<long, func_info> run_llvmbolt(const ocolos_env*);




/*
 * Read the output from `nm` to get the starting address, 
 * original functions size, function name etc. from `nm`'s 
 * output. 
 * The output's key is the starting address of the function.
 */
unordered_map<long, func_info> get_func_with_original_addr(const ocolos_env*);




/*
 * Get the functions that are not moved by BOLT by checking 
 * and comparing both the original binary and the BOLTed 
 * binary.
 */ 
unordered_map<long, func_info> get_unmoved_func(unordered_map<long, func_info> func_with_addr, unordered_map<long, func_info> bolted_func);




/*
 * Change unordered_map format of the functions info into
 * map format, since map in C++ is heap and provides a better
 * way for checking the bound of a function. 
 */
map<long, func_info> change_func_to_heap(unordered_map<long, func_info> unmoved_func);




/*
 * Get vtable from nm's output. The first value (key) stores
 * the starting address of a v-table entry, and the second 
 * value (value) stores the ending address of a v-table entry.
 */
unordered_map<string, string> get_v_table(const ocolos_env*);




/*
 * Get the detailed function info based on the ip of the functions
 * on call stack (this is actually the ip of the caller functions's 
 * call instruction), and the heap which contains all functions' 
 * information.
 * The return value is a list of the caller functions' info.
 * (These caller functions should also be on the call stack.)
 */
unordered_map<long, func_info> get_func_in_call_stack(vector<unw_word_t> call_stack_ips, map<long, func_info> unmoved_func_heap);



/*
 * Store the starting address of functions that are on the call stack
 * into a binary file. 
 * This information is useful for when perf2bolt works on the profile 
 * collected from C_x round. It will be read by perf2bolt in the next 
 * round of code layout optimization 
 */
void write_func_on_call_stack_into_file(const ocolos_env* ocolos_environment,
                                        unordered_map<long, func_info> func_in_call_stack);



/*
 * Get the functions that are not moved by BOLT, nor are they on
 * the call stack.   
 */
unordered_map<long, func_info> get_unmoved_func_not_in_call_stack(unordered_map<long, func_info>func_in_call_stack, unordered_map<long, func_info> unmoved_func);	



