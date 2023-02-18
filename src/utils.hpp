#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <unordered_set>
#include <vector>
#include <string>
#include <cstring>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <map>
#include <wait.h>
#include <libunwind.h>
#include <libunwind-x86_64.h>
#include <libunwind-ptrace.h>
#include <signal.h>
#include <pthread.h>
#include <thread>
#include <dirent.h>
#include <arpa/inet.h>
#include <unordered_map>
#include <ctype.h>
#include <fcntl.h> 
#include <iomanip>
#include <chrono>
#include <filesystem>

#define panic(X) fprintf(stderr, #X "\n");

using namespace std;

extern char **environ;

#ifndef UTILS
#define UTILS


/*
 * struct that keep track of the functions
 * extracted from the original and BOLTed binary.
 */
typedef struct func_info{
	bool in_call_stack;
	long original_addr;
	long moved_addr;
	long original_size;
	long moved_size;
	string func_name;
	string orig_addr_str;
	string moved_addr_str;
} func_info;




/* 
 * struct that stores all the paths and commands 
 * that are read from the config file. 
 */
typedef struct ocolos_env{
   unordered_map<string, string> configs;

   string dir_path; 
   string bolted_binary_path;
   string ld_preload_path;
   string bolted_function_bin;

   string call_sites_bin;
   string v_table_bin;
   string unmoved_func_bin;

   string call_sites_all_bin;
   string call_sites_list_bin;

   string perf_path;
   string nm_path;
   string objdump_path;
   string llvmbolt_path;
   string perf2bolt_path;

   string target_binary_path;
   string run_server_cmd;
   string init_benchmark_cmd;
   string run_benchmark_cmd;

   string client_binary_path;
   string db_name;
   string db_user_name;

   string lib_path;
   string tmp_data_path;

   int listen_fd;

   /*
    * Constructor function that initialize all strings
    * stored in this struct.
    */
   ocolos_env();



   /*
    * Read the config file and store all information 
    * into key-value store. The keys are: 
    * (1) the name of the binary
    *     in this case the value is the absolute path
    *     of the binary 
    * (2) the abbreviation of the command
    *     in this case the vlue is the full command.
    */
   void read_config();



   /*
    * Get the absolute path of the current directory
    * where the `tracer` locates.
    */
   void get_dir_path();



   /*
    * Get the absolute path of a binary. The binary's
    * name is specified as the argument of this 
    * function. 
    */
   string get_binary_path(string);



   /*
    * Extract the target server binary from the command 
    * specified in the config file. 
    */
   void get_target_binary_path();




   /*
    * Read the command from the config file, including 
    * (1) the command of the server process, (2) the 
    * command of initialize the benchmark, (3)the 
    * command of running the benchmark.
    */
   void get_cmds();
} ocolos_env;

#endif




/*
 * Test whether a string is a decimal number
 * return true: is a decimal number
 * return false: is not a decimal number
 */
bool is_num(string);




/*
 * Convert a string that is a decimal number
 * to a long int.
 * The return value is the long int. 
 */
long convert_str_2_int(string);




/*
 * Split a string by tab and space and then store
 * the splitted strings into vector<string>
 */
vector<string> split_line(string str);




/*
 * Split a string by a delimiter and then store 
 * the splitted strings into vector<string>
 */
vector<string> split(const string &s, char delim);




/*
 * Split a string by tab and space and then store
 * the splitted strings into char**.
 */
char** split_str_2_char_array(const string &str);




/*
 * Test whether a string is a hexadecimal number
 * return true: is a hexadecimal number
 * return false: is not a hexadecimal number
 */
bool is_hex(string);




/*
 * Convert a string that is a hexadecimal number
 * to a long int.
 * The return value is the long int. 
 */
long convert_str_2_long(string str);




/*
 * Extract all 2-digit hexadecimal number in the
 * string and then store them into vector<uint8>.
 * This is used for extract machine code from 
 * `objdump`'s output.
 */
uint8_t convert_str_2_uint8(string);




/*
 * Convert a long int to a 4 uint8_t numbers, and 
 * push them from the highest digit to the lowest
 * digit to a vector<uint8_t>.
 * This is used to patch call sites in the machine
 * code extracted from objdump. 
 */
vector<uint8_t> convert_long_2_vec_uint8(long);




/*
 * Convert a long integer to to a string that shows
 * the value of the long interger in hexadecimal.
 * This is used to patch call sites in the machine 
 * code extracted from objdump. 
 */
string convert_long_2_hex_string(long);




/*
 * Get the keys from unordered_map, if the keys are 
 * long int type. And then store all keys into a 
 * vector. The keys are actually the starting 
 * addresses of functions.
 * This is used to pass all starting addresses 
 * of unmoved functions in the BOLTed binary to the 
 * Rust code.  
 */
vector<long> get_keys_to_array(unordered_map<long, func_info> func);




/*
 * Get the moved addresses (the starting addresses of 
 * functions in the BOLTed binary that have starting 
 * address changed) from unordered_map.
 * This is used to pass all starting addresses of the 
 * moved functions in the BOLTed binary to the Rust
 * code.  
 */
vector<long> get_moved_addr_to_array(unordered_map<long, func_info> func);




/*
 * Delete all intermediate files.
 */
void clean_up(const ocolos_env*);
