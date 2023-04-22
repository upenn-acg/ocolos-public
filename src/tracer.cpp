#include "infrastructure.hpp"
#include "extract_machine_code.hpp"
#include "ptrace_pause.hpp"

using namespace std;

int main(){
   // TCP resources initialization
   // TCP connections is used for getting the 
   // starting address of the function in 
   // ld_preload
   ocolos_env ocolos_environ;
   struct sockaddr_in servaddr;
   create_tcp_socket(ocolos_environ.listen_fd, servaddr);

   // spawn target process
   pid_t target_pid = fork();
   if (target_pid == 0){
      // child - target server process
      create_target_server_process(&ocolos_environ);
      printf("[tracer] target server process creation fails\n");
   }
   else {
      // parent - tracer process
      // wait for child to send the addr of library 
      // function back via TCP connection 
      void* lib_addr = get_lib_addr(ocolos_environ.listen_fd);		
      thread t = thread(send_data_path, &ocolos_environ);
      initialize_benchmark(&ocolos_environ);
      run_benchmark(&ocolos_environ);
      sleep(20);
      // run perf and BOLT
      // (1) get perf.data
      // (2) get perf.fdata
      // (3) generate the BOLTed binary 
      // (4) get the set of functions that is BB reordered by 
      //     BOLT.
      run_perf_record(target_pid, &ocolos_environ);
      run_perf2bolt(&ocolos_environ);
      unordered_map<long, func_info> bolted_func = run_llvmbolt(&ocolos_environ);

      // get all functions that have location changed		
      unordered_map<long, func_info> func_with_addr = get_func_with_original_addr(&ocolos_environ);
      unordered_map<long, func_info> unmoved_func = get_unmoved_func(func_with_addr, bolted_func);	
      map<long, func_info> func_heap = change_func_to_heap(func_with_addr);
      unordered_map<string, string> v_table = get_v_table(&ocolos_environ);

      // delete the old binary files for code replacement
      // create the num files for code replecement before pausing 
      // the target process 
      string delete_all_bin = "rm -rf "+
                              ocolos_environ.bolted_function_bin+" "+
                              ocolos_environ.v_table_bin+" "+
                              ocolos_environ.call_sites_bin+" "+
                              ocolos_environ.unmoved_func_bin;
      if (system(delete_all_bin.c_str())==-1) exit(-1);

      FILE *pFile1;
      pFile1 = fopen(ocolos_environ.call_sites_bin.c_str(), "a");

      vector<long> addr_bolted_func = get_moved_addr_to_array(bolted_func);
      write_functions ( ocolos_environ.bolted_binary_path.c_str(), 
                        ocolos_environ.bolted_function_bin.c_str(), 
                        addr_bolted_func.data(), 
                        addr_bolted_func.size() );

      write_vtable(ocolos_environ.bolted_binary_path.c_str(), 
                   ocolos_environ.v_table_bin.c_str() );

    
      #ifdef TIME_MEASUREMENT
      auto begin = std::chrono::high_resolution_clock::now();
      #endif



      // to pause all running threads of the target process
      // and then get the PIDs(tid) of these threads
      vector<pid_t> tids = pause_and_get_tids(target_pid);

      // unwind call stack and get the functions
      // in the call stacks of each threads
      vector<unw_word_t> call_stack_ips = unwind_call_stack(tids);
      unordered_map<long, func_info> func_in_call_stack =  get_func_in_call_stack(call_stack_ips, func_heap);
      unordered_map<long, func_info> unmoved_func_not_in_call_stack = get_unmoved_func_not_in_call_stack(func_in_call_stack, unmoved_func);

      // for continuous optimization
      write_func_on_call_stack_into_file(&ocolos_environ, func_in_call_stack);   

      // extract the machine code of each function
      // from the output of objdump
      vector<long> addr_unmoved_func_not_in_call_stack = get_keys_to_array(unmoved_func_not_in_call_stack); 
      extract_call_sites(pFile1, bolted_func, func_in_call_stack, &ocolos_environ);
		
      write_functions(ocolos_environ.bolted_binary_path.c_str(), ocolos_environ.unmoved_func_bin.c_str(), addr_unmoved_func_not_in_call_stack.data(), addr_unmoved_func_not_in_call_stack.size());
	

      fflush(pFile1);
      fclose(pFile1);

      // change the IP of the target process to be 
      // the starting address of our library code 
      // then make the target process to execute 
      // the lib code to insert machine code
      struct user_regs_struct regs, old_regs;
      struct user_fpregs_struct fregs;
      vector<pid_t> tids_have_code_insertion;
      for (unsigned i=0; i<tids.size(); i++){
         if(!ptrace_single_step(tids[i], lib_addr, regs, old_regs, fregs)){
            continue;
         }
         ptrace_cont(tids[i], regs, old_regs, fregs);
         break;			
      }

      #ifdef DEBUG	
      // deliver a SIGSTOP signal to target process. 
      // before resume the target process, so that 
      // we can attach GDB to target process later on.
      for (unsigned i=0; i<tids.size(); i++){
         int rc = syscall(SYS_tgkill, tids[0], tids[i], SIGSTOP);	
      }
      #endif


      // ptrace detach all threads of the
      // target process
      for (unsigned i=0; i<tids.size(); i++){
         ptrace(PTRACE_DETACH, tids[i], NULL, NULL);	
      }

      #ifndef DEBUG_INFO
      clean_up(&ocolos_environ);
      #endif

      #ifdef TIME_MEASUREMENT
      auto end = std::chrono::high_resolution_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
      printf("[tracer][time] machine code insertion took %f seconds to execute \n", elapsed.count() * 1e-9);
      #endif

      t.join();      
      printf("[tracer][OK] code replacement done!\n");
      #ifdef DEBUG
      while(true);
      #endif

      // continuous optimization
      // the perf record will collect profile from the C1 round text section
      // the perf.data collected from C1 round together with 
      // (1) BOLTed binary produced from C0 roound + 
      // (2) callstack_func.bin (the function on the call stack when C0 round code replacement is performed) +
      // (3) the info of BOLTed binary (BOLTed text section's starting address)
      // will be sent to llvm-bolt to produce a C1 round BOLTed binary.
      // C1 round's BOLTed binary is used for C1 round's code replacement
      #ifdef CONT_OPT
      run_perf_record(target_pid, &ocolos_environ);
      #endif
   }
}



