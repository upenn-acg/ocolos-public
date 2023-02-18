#define UNW_LOCAL_ONLY
#define LD_PRELOAD_PATH "LD_PRELOAD=replace_function.so\0"
#define MMAP_PAGE_SIZE 4*1024 // huge page: 2*1024*1024
#define MMAP_PAGE_OFFSET 0b111111111111 // huge page: 0b111111111111111111111


#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cpuid.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string>
#include <cstring>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <fstream>
#include <libunwind.h>
#include <signal.h>
#include <sys/ptrace.h>
#include <iostream>
#include <filesystem>

using namespace std;

extern char** environ;
/* 
 * struct that stores all the paths and commands that are read from the 
 * config file. 
 */
typedef struct ocolos_env{
   unordered_map<string, string> configs;

   string tmp_data_path; 

   string bolted_function_bin;
   string call_sites_bin;
   string v_table_bin;
   string unmoved_func_bin;

   string debug_log;
   /*
    * Constructor function that initialize all strings stored in this struct.
    */
   ocolos_env(){}

   /*
    * Get the absolute path of the current directory where the data to be 
    * inserted locates.
    */
   void get_dir_path(string);

} ocolos_env;

extern ocolos_env ocolos_environ;



/* 
 * this function runs before the real server image is loaded into the 
 * address space. It detects the starting address of insert_machine_code()
 * and then setup a tcp socket to tell ocolos the starting address of the 
 * insert_machine_code()
 */ 
void before_main(void) __attribute__((constructor (101)));




/*
 * The main functions of inserting machine code to the target process
 * It includes connecting to ocolos to acquire the path where the data
 * to be inserted locates, invoking the insert_BOLTed_function(), 
 * insert_call_site(), insert_v_table(), insert_unmoved_function(), 
 * and then raise a signal to ocolos, indicating the lib code finished
 * executing.  
 */
void insert_machine_code(void);




/*
 * create a tcp socket for receiving message sending from Ocolos' tracer. 
 */
void create_tcp_socket(int & listen_fd, struct sockaddr_in & servaddr);



/*
 * Receive the data_path from the message sending from Ocolos' tracer.
 */
string get_data_path(int listen_fd);




/*
 * Convert a string into a long integer type. 
 */
uint64_t convert_str_2_long(string str);




/*
 * First using mmap() to allocate new memory pages and then copy BOLTed 
 * code (machine code) to those pages.
 */
void insert_BOLTed_function(FILE* pFile, FILE* recordFile, long base_n);




/*
 * insert_call_site(), insert_v_table(), insert_unmoved_function() all 
 * invoke insert_code_to_orig_text_sec(), which inserts machine code to 
 * the origianl text section. 
 */
void insert_call_site();
void insert_v_table();
void insert_unmoved_function();
void insert_code_to_orig_text_sec(FILE* pFile, FILE* recordFile, long base_n);




/*
 * For debug usage. Print the function that causes the error and when the 
 * error occurs, print the starting address of the function that is being 
 * inserted and other information about that function. 
 * This is mainly used for debuggin whether mmap() and mprotect() works 
 * correctly. 
 */
void print_err_and_exit(FILE* recordFile, string func, long addr, long len, long page);




/*
 * For debug usage. The inserted machine to be inserted are stored in 
 * uint8_t format string into the recordFile.
 */
void record_machine_code(FILE* recordFile, uint8_t* machine_code, unsigned int len);





/*
 * change __libc_start_main to move the location of the heap
 */

extern "C" int __libc_start_main(void *orig_main, int argc, char* argv[],
  void (*init_func)(void),
  void (*fini_func)(void),
  void (*rtld_fini_func)(void),
  void *stack_end);

