#include "infrastructure.hpp"

using namespace std;



void initialize_benchmark(const ocolos_env* ocolos_environ){
   sleep(20);
   string command = ocolos_environ->client_binary_path + " -u "+
                    ocolos_environ->db_user_name+" -e 'drop database "+
                    ocolos_environ->db_name+"'";
   if ( system (command.c_str())) exit(-1);
   command = ocolos_environ->client_binary_path + " -u "+
             ocolos_environ->db_user_name+" -e 'create database "+
             ocolos_environ->db_name+"'";

   if ( system (command.c_str())==-1) exit(-1);
   sleep(3);

   pid_t sysbench_init_pid = fork();
   if (sysbench_init_pid == 0){
      // child
      string benchmark_cmd = ocolos_environ->init_benchmark_cmd;
      char** argv = split_str_2_char_array(benchmark_cmd);
      char **envp = environ;
      execve(argv[0], argv, envp);
   }
   int stat;
   waitpid(sysbench_init_pid,&stat,0);
}



void run_benchmark(const ocolos_env* ocolos_environ){
   pid_t sysbench_pid = fork();

   if (sysbench_pid == 0){
      // child
      int fd = open("sysbench_output.txt",O_WRONLY | O_CREAT, 0666);
      dup2(fd, 1);
      string benchmark_cmd = ocolos_environ->run_benchmark_cmd;
      char** argv = split_str_2_char_array(benchmark_cmd);	
      char **envp = environ;
      execve(argv[0], argv, envp);
   }
}




void create_target_server_process(const ocolos_env* ocolos_environ){
   string ld_pre = "LD_PRELOAD="+ ocolos_environ->ld_preload_path;
   char* ldpreload = new char[ld_pre.length()+1];
   strcpy(ldpreload, ld_pre.c_str());

   string exe_cmd = ocolos_environ->run_server_cmd;
   char** argv = split_str_2_char_array(exe_cmd);
   putenv(ldpreload);
   char **envp = environ;
   execve(argv[0], argv, envp);
}



void create_tcp_socket(int listen_fd, struct sockaddr_in & servaddr){
   bzero(&servaddr, sizeof(servaddr));
   servaddr.sin_family = AF_INET;
   servaddr.sin_addr.s_addr = htons(INADDR_ANY);
   servaddr.sin_port = htons(8000);
   bind(listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr));
   listen(listen_fd, 10);
}





void* get_lib_addr(int listen_fd){
   struct sockaddr_in clientaddr;
   socklen_t clientaddrlen = sizeof(clientaddr);
   int comm_fd = accept(listen_fd, (struct sockaddr*)&clientaddr, &clientaddrlen);
   printf("[tracer] connection from %s\n", inet_ntoa(clientaddr.sin_addr));
   char* buf = (char*)malloc(100*sizeof(char));
   int n = read(comm_fd, buf, 50);
   if (n<=0){
      printf("[tracer] error in receiving msg from tracee.\n");
   }
   string addr(buf);
   void* lib_addr = (void*)(convert_str_2_int(addr));		
   printf("[tracer] the received address is: %p\n", lib_addr);
   close(listen_fd);
   return lib_addr;
}




void send_data_path(const ocolos_env* ocolos_environ){
   int listen_fd = socket(PF_INET, SOCK_STREAM, 0);
   struct sockaddr_in servaddr;
   bzero(&servaddr, sizeof(servaddr));
   servaddr.sin_family = AF_INET;
   servaddr.sin_addr.s_addr = htons(INADDR_ANY);
   servaddr.sin_port = htons(9000);
   bind(listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr));
   listen(listen_fd, 10);

   socklen_t servaddrlen = sizeof(servaddr);
   int comm_fd = accept(listen_fd, (struct sockaddr*)&servaddr, &servaddrlen);
   printf("[tracer] connection from %s\n", inet_ntoa(servaddr.sin_addr));


   int sockfd = socket(PF_INET, SOCK_STREAM, 0);
   if (sockfd < 0) {
      printf("[tracer] cannot open socket (%s)\n", strerror(errno));
      exit(-1);
   }

   // open a socket, connect to the tracer's parent process
   // and sends the address of insert_machine_code() to 
   // tracer
   struct sockaddr_in servaddr2;
   bzero(&servaddr2, sizeof(servaddr2));
   servaddr2.sin_family = AF_INET;
   servaddr2.sin_port = htons(18000);
   inet_pton(AF_INET, "localhost", &(servaddr2.sin_addr));
   connect(sockfd, (struct sockaddr*)&servaddr2, sizeof(servaddr2));

   // convert the virtual address of insert_machine_code()
   string path = ocolos_environ->tmp_data_path;
   char* buf = (char*) malloc(sizeof(char)*path.size());
   strcpy(buf, path.c_str());
   int n = write(sockfd, buf, path.size());
   if (n <= 0) exit(-1);	
   close(sockfd);
   close(comm_fd);
}




void run_perf_record(int target_pid, const ocolos_env* ocolos_environ){
   static int func_exec_count = 0;
   func_exec_count++;
   string command;
   if (func_exec_count==1){
      command = ocolos_environ->perf_path+
                       " record -e cycles:u -j any,u -a -o "+
                       ocolos_environ->tmp_data_path+ 
                       "perf.data -p "+
                       to_string(target_pid)+
                       " -- sleep 60";
   }
   else{
      stringstream ss;
      ss << func_exec_count;
      string f_exec_count = ss.str();
      command = ocolos_environ->perf_path+
                       " record -e cycles:u -j any,u -a -o "+
                       ocolos_environ->tmp_data_path+ 
                       "perf"+f_exec_count+".data -p "+
                       to_string(target_pid)+
                       " -- sleep 60";

   }
   char * command_cstr = new char [command.length()+1];
   strcpy (command_cstr, command.c_str());
   if (system(command_cstr)!=0) printf("[tracer] error in %s\n",__FUNCTION__);
}




void run_perf2bolt(const ocolos_env* ocolos_environ){
   #ifdef TIME_MEASUREMENT
   auto begin = std::chrono::high_resolution_clock::now();
   #endif

   static int func_exec_count = 0;
   func_exec_count++;

   FILE *fp1;
   char path1[3000];

   if (func_exec_count==1){
      string command = ""+ocolos_environ->perf2bolt_path+" -p "+
                       ocolos_environ->tmp_data_path+
                       "perf.data -o "+
                       ocolos_environ->tmp_data_path+
                       "perf.fdata "+ ocolos_environ->target_binary_path; 
      char* command_cstr = new char [command.length()+1];
      strcpy (command_cstr, command.c_str());


      fp1 = popen(command_cstr, "r");

      if (fp1 == NULL){
         printf("[tracer] fail to run perf2bolt\n" );
         exit(-1);
      }

      vector<string> BOLTed_addr;
      bool has_sharp = false;
      while (fgets(path1, sizeof(path1), fp1) != NULL) {
         string line(path1);
         cout<<line;
         vector<string> words = split_line(line);
         if (words.size()>1){
            if (words[0]=="####"){
               printf("[tracer] we've received #### !!!!!\n");
               has_sharp = true;
               BOLTed_addr.push_back(words[1]);
            }
         }
      }

      if (has_sharp){
         string file_path = ocolos_environ->tmp_data_path+"BOLTed_bin_info.txt";
         FILE* fp = fopen(file_path.c_str(), "w");
         fprintf(fp, "%s\n", ocolos_environ->bolted_binary_path.c_str());
         fprintf(fp, "%s\n", ocolos_environ->target_binary_path.c_str());
         for (unsigned i=0; i<BOLTed_addr.size(); i++){
            fprintf(fp, "%s\n", BOLTed_addr[i].c_str());
         }
         fflush(fp);
         fclose(fp);
        
      }
		
      fclose(fp1);

   }
   else {
      stringstream ss;
      ss << func_exec_count;
      string f_exec_count = ss.str();

      string command = ""+ocolos_environ->perf2bolt_path+
                       " --ignore-build-id --cont-opt"+
                       " -p "+
                       ocolos_environ->tmp_data_path+
                       "perf"+f_exec_count+".data -o "+
                       ocolos_environ->tmp_data_path+
                       "perf"+f_exec_count+".fdata "+ 
                       ocolos_environ->target_binary_path; 
      char* command_cstr = new char [command.length()+1];
      strcpy (command_cstr, command.c_str());

      fp1 = popen(command_cstr, "r");

      if (fp1 == NULL){
         printf("[tracer] fail to run perf2bolt\n" );
         exit(-1);
      }

      while (fgets(path1, sizeof(path1), fp1) != NULL) {
         string line(path1);
         cout<<line;
         vector<string> words = split_line(line);
         if (words.size()>1){
            if (words[0]=="####"){
               printf("[tracer] we've received #### !!!!!\n");
               string file_path = ocolos_environ->dir_path+"BOLTed_bin_info.txt";
               FILE* fp = fopen(file_path.c_str(), "w");
               fprintf(fp, "%s\n", ocolos_environ->bolted_binary_path.c_str());
               fprintf(fp, "%s", words[1].c_str());
               fflush(fp);
               fclose(fp);
            }
         }
      }
		
      fclose(fp1);


   }
   #ifdef TIME_MEASUREMENT
   auto end = std::chrono::high_resolution_clock::now();
   auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
   printf("[tracer(time)] perf2bolt took %f seconds to execute \n", elapsed.count() * 1e-9);
   #endif
}




unordered_map<long, func_info> run_llvmbolt(const ocolos_env* ocolos_environ){
   #ifdef TIME_MEASUREMENT
   auto begin = std::chrono::high_resolution_clock::now();
   #endif

   FILE *fp1;
   char path1[3000];
   string command = ""+ocolos_environ->llvmbolt_path+" " +
                    ocolos_environ->target_binary_path + 
                    " -b "+
                    ocolos_environ->tmp_data_path+
                    "perf.fdata -o " +
                    ocolos_environ->bolted_binary_path + 
                    " --enable-bat --enable-ocolos"+
                    " --enable-func-map-table "+
                    " -reorder-blocks=cache+ "+
                    "-reorder-functions=hfsort "+
                    "-split-functions=0 "+
                    "-dyno-stats";
   char* command_cstr = new char[command.length()+1];
   strcpy(command_cstr, command.c_str()); 
   fp1 = popen(command_cstr, "r");

   if (fp1 == NULL){
      printf("Failed to run llvm-bolt command\n" );
      exit(-1);
   }

   // collect the bolted function name 
   // from BOLT's output
   unordered_map<long, func_info> changed_functions;
   while (fgets(path1, sizeof(path1), fp1) != NULL) {
      string line(path1);
      vector<string> words = split_line(line);
      if (words.size()>1){
         if(words[0]=="@@@@"){
            func_info new_func;
            new_func.func_name = words[1];
            new_func.orig_addr_str = words[2];
            new_func.moved_addr_str = words[3];
            new_func.original_addr = convert_str_2_long(words[2]);
            new_func.moved_addr = convert_str_2_long(words[3]);
            new_func.original_size = convert_str_2_long(words[4]);
            new_func.moved_size = convert_str_2_long(words[5]);
            changed_functions[convert_str_2_long(words[2])] = new_func;
         }
      }
   }
		
   printf("[tracer] %ld functions was moved (functions reordered) by BOLT\n", changed_functions.size());
   fclose(fp1);

   #ifdef TIME_MEASUREMENT
   auto end = std::chrono::high_resolution_clock::now();
   auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
   printf("[tracer(time)] llvm-bolt took %f seconds to execute \n", elapsed.count() * 1e-9);
   #endif

   return changed_functions;
}



unordered_map<long, func_info> get_func_with_original_addr(const ocolos_env* ocolos_environ){
   FILE *fp;
   char path[3000];
   string command = "" + ocolos_environ->nm_path + " -S -n "+ ocolos_environ->target_binary_path;
   char* command_cstr = new char[command.length()+1];
   strcpy (command_cstr, command.c_str());
   fp = popen(command_cstr, "r");
   if (fp == NULL){
      printf("Failed to run nm on original binary\n" );
      exit(-1);
   }

   vector<string> lines;
   while (fgets(path, sizeof(path), fp) != NULL) {
      string line(path);
      lines.push_back(line);
   }

   unordered_map<long, func_info>	func_with_addr;
   for (unsigned i=0; i<lines.size(); i++){
      string line = lines[i];
      vector<string> words = split_line(line);
      if (words.size()>3){
         if ((words[2]=="T")||(words[2]=="t")){
            long addr = convert_str_2_long(words[0]);
            if (func_with_addr.find(addr)==func_with_addr.end()){
               func_info new_func;
               new_func.original_addr = addr;
               new_func.original_size = convert_str_2_long(words[1]);
               new_func.orig_addr_str = words[0];
               func_with_addr[addr] = new_func;
            }
            else {

            }
         }
      }
   }
   #ifdef MEASUREMENT
   printf("[tracer] %ld functions are in the target process \n", func_with_addr.size());
   #endif
   fclose(fp);
   return func_with_addr;
}



unordered_map<long, func_info> get_unmoved_func(unordered_map<long, func_info> func_with_addr, 
                                                unordered_map<long, func_info> bolted_func)
{
   unordered_map<long, func_info> unmoved_func;
   for(auto it = func_with_addr.begin(); it != func_with_addr.end(); it++){
      if (bolted_func.find(it->first)==bolted_func.end()){
         unmoved_func[it->first] = it->second;
      }
   }
   #ifdef MEASUREMENT
   printf("[tracer(measure)] the size of unmoved_func is: %lu\n",unmoved_func.size());
   printf("[tracer(measure)] the size of orig_func is: %lu\n",func_with_addr.size());
   #endif
   return unmoved_func;
}



map<long, func_info> change_func_to_heap(unordered_map<long, func_info> func){
   map<long, func_info> func_heap;
   for(auto it = func.begin(); it != func.end(); it++){
      func_heap[it->first] = it->second;
   }
   return func_heap;
}



unordered_map<string, string> get_v_table(const ocolos_env* ocolos_environ){
   FILE *fp3;
   char path3[3000];
   string command = "" + ocolos_environ->nm_path + " -n -C " + ocolos_environ->bolted_binary_path;	
   char* command_cstr = new char[command.length()+1];
   strcpy (command_cstr, command.c_str());
   fp3 = popen(command_cstr, "r");
   if (fp3 == NULL){
      printf("[tracer] Failed to run nm on BOLTed binary\n" );
      exit(-1);
   }

   unordered_map<string, string> v_table;
   vector<string> lines;
   while (fgets(path3, sizeof(path3), fp3) != NULL) {
      string line(path3);
      lines.push_back(line);
   }
   for (unsigned i=0; i<lines.size(); i++ ){
      string line = lines[i];
      vector<string> words = split_line(line);
      if (words.size()>3){
         if (words[2]=="vtable"){
            string next_line = lines[i+1];
            vector<string> next_line_words = split_line(next_line);
            v_table.insert(make_pair(words[0], next_line_words[0]));
         }
         else if ((words.size()>4)&&(words[2]=="construction")&&(words[3]=="vtable")){
            string next_line = lines[i+1];
            vector<string> next_line_words = split_line(next_line);
            v_table.insert(make_pair(words[0], next_line_words[0]));
         }
      }
   }
   fclose(fp3);

   return v_table;
}




unordered_map<long, func_info> get_func_in_call_stack(vector<unw_word_t> call_stack_ips, 
                                                      map<long, func_info> unmoved_func_heap)
{
   unordered_map<long, func_info> func_in_call_stack;
   for (unsigned i = 0; i < call_stack_ips.size(); i++){
      auto it_low = unmoved_func_heap.lower_bound(call_stack_ips[i]);
      if (it_low!=unmoved_func_heap.end()){
         auto it = prev(it_low);
         func_in_call_stack[it->first] = it-> second;
      }
   }
   #ifdef MEASUREMENT
   printf("[tracer] number of function in call stack: %u\n",func_in_call_stack.size());
   #endif
   return func_in_call_stack;
}


void write_func_on_call_stack_into_file(const ocolos_env* ocolos_environment, 
                                        unordered_map<long, func_info> func_in_call_stack){
  string snapshot_path = ocolos_environment->tmp_data_path + "callstack_func.bin";
  FILE* fp = fopen(snapshot_path.c_str(), "w");

  long callstack_func_number = func_in_call_stack.size();
  fwrite(&callstack_func_number, sizeof(long), 1, fp);

  uint64_t buffer[func_in_call_stack.size()];
  int i = 0; 
  for (auto it = func_in_call_stack.begin(); it != func_in_call_stack.end(); it++){
     buffer[i] = (uint64_t)it->first;
     i++;
  }
  fwrite(buffer, sizeof(uint64_t), callstack_func_number, fp);

  fflush(fp);
  fclose(fp);
}


unordered_map<long, func_info> get_unmoved_func_not_in_call_stack(unordered_map<long, func_info>func_in_call_stack, 
                                                                  unordered_map<long, func_info> unmoved_func)
{
   unordered_map<long, func_info> unmoved_func_not_in_call_stack;
   for (auto it=unmoved_func.begin(); it!=unmoved_func.end(); it++){
      if (func_in_call_stack.find(it->first)==func_in_call_stack.end()){
         unmoved_func_not_in_call_stack[it->first] = it->second;
      }
   }
   return unmoved_func_not_in_call_stack;
}
