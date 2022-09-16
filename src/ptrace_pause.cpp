#include "ptrace_pause.hpp"

using namespace std;

vector<pid_t> pause_and_get_tids(pid_t target_pid){
   vector<pid_t> tids;
   unordered_set<pid_t> tids_set;
   while (true) {
      string path_cpp = "/proc/"+to_string(target_pid)+"/task";
      char * path = new char [path_cpp.length()+1];
      strcpy (path, path_cpp.c_str());

      struct dirent *entry;
      DIR *dir = opendir(path);
      if (dir == NULL) {
         exit(-1);
      }

      bool has_new_tid = false;
      while ((entry = readdir(dir)) != NULL) {
         string tid(entry->d_name);
         if (is_num(tid)) {
            pid_t tid_number = convert_str_2_int(tid);
            if (tids_set.find(tid_number)==tids_set.end()){
               tids.push_back((pid_t) tid_number);
               tids_set.insert(tid_number);
               has_new_tid = true;
            }
         }
      }
      closedir(dir);
      if (!has_new_tid) break;
   }
   for (unsigned i=0; i<tids.size(); i++){
      ptrace(PTRACE_ATTACH, tids[i], 0, 0);
   }

   for (unsigned i=0; i<tids.size(); i++){
      int status;
      waitpid(tids[i], &status, 0);
   }
   return tids;
}



vector<unw_word_t> unwind_call_stack(vector<pid_t> tids){
   #ifdef DEBUG_INFO
   printf("\n-------------call stack------------\n");
   #endif

   vector<unw_word_t> call_stack;
   for (unsigned i=0; i<tids.size(); i++){
      unw_addr_space_t as;
      struct UPT_info *ui;
      unw_cursor_t c;

      as = unw_create_addr_space(&_UPT_accessors, 0);
      if (!as) panic("unw_create_addr_space failed");

      ui = (struct UPT_info*) _UPT_create(tids[i]);
      if (!ui) panic("_UPT_create failed");
      int rc = unw_init_remote(&c, as, ui);
      if (rc != 0) {
         if (rc == UNW_EINVAL) {
            panic("unw_init_remote: UNW_EINVAL");
         } else if (rc == UNW_EUNSPEC) {
            panic("unw_init_remote: UNW_EUNSPEC");
         } else if (rc == UNW_EBADREG) {
            panic("unw_init_remote: UNW_EBADREG");
         } else {
            panic("unw_init_remote: UNKNOWN");
         }
      }

      do {
         unw_word_t  offset, ip, sp;
         char fname[4096];
         unw_get_reg(&c, UNW_REG_IP, &ip);
         unw_get_reg(&c, UNW_REG_SP, &sp);
         fname[0] = '\0';
         (void) unw_get_proc_name(&c, fname, sizeof(fname), &offset);
         string f_name(fname);
         call_stack.push_back(ip);

         #ifdef DEBUG_INFO
         printf("[tracer] %p : (%s+0x%x) \n\t\t[sp=%p]\n", (void *)ip,fname,(int) offset,(void *)sp);
         #endif
      } while (unw_step(&c) > 0);

      _UPT_destroy(ui);
   }
   
   #ifdef DEBUG_INFO
   printf("-----------------------------------\n\n");
   #endif

   return call_stack;
}






	
bool ptrace_single_step(pid_t tid, void* lib_addr, struct user_regs_struct &regs, struct user_regs_struct &old_regs, struct user_fpregs_struct &fregs){
   // setting the IP points to function in library
   ptrace(PTRACE_GETREGS,tid,NULL, &regs);
   ptrace(PTRACE_GETFPREGS, tid, NULL, &fregs);
   old_regs = regs;
   regs.rip = (long)lib_addr;
   regs.rsp = ((regs.rsp - 256) & 0xFFFFFFFFFFFFFFF0) - 8;

   if (ptrace(PTRACE_SETREGS, tid, NULL, &regs)!=0){
      printf("[tracer] error in PTRACE_SETREGS\n");
      exit(-1);
   }

   ptrace(PTRACE_GETREGS, tid, NULL, &regs);

   #ifdef DEBUG_INFO
   printf("[tracer] thread id = %d, rip = %llx\n", tid, old_regs.rip);	
   printf("[tracer] before SINGLESTEP, set RIP = %llx (lib addr)\n", regs.rip);
   #endif

   int status;
   ptrace(PTRACE_SINGLESTEP, tid, NULL, 0);
   waitpid(tid, &status, 0);

   if (!WIFSTOPPED(status)){
      if (WIFEXITED(status)) {
         int num = WEXITSTATUS(status);
         printf("[tracer][error] tracee exits normally, exit num = %d\n", num);
      }
      if (WIFSIGNALED(status)){
         int num = WTERMSIG(status);
         printf("[tracer][error] tracee deliviers a signal, sig num = %d\n", num);
      }
      exit(-1);
   }
   else {
      int sig = WSTOPSIG(status);	
      if (sig != SIGTRAP){
         char *sig_str = strdup(_sys_siglist[sig]);

         #ifdef DEBUG_INFO
         printf("[tracer][error] tracee deliver a signal %s\n", sig_str);
         printf("[tracer][error] this is wrong, the signal delivered should be SIGTRAP\n");
         #endif

         if (ptrace(PTRACE_SETREGS, tid, NULL, &old_regs)!=0){
            printf("[tracer][error] error in PTRACE_SETREGS\n");
            exit(-1);
         }
         if (ptrace(PTRACE_SETFPREGS, tid, NULL, &fregs)!=0){
            printf("[tracer][error] error in PTRACE_SETFPREGS\n");
            exit(-1);
         }
         return false;
      }
   }
	ptrace(PTRACE_GETREGS, tid, NULL, &regs);
   #ifdef DEBUG_INFO
   printf("[tracer] receive SIGSTOP from tracee (lib code), tracee finished a SINGLESTEP!\n");
   printf("[tracer] after SINGLESTEP, RIP = %llx\n", regs.rip);
   #endif

   if (regs.rip <= (long long unsigned)lib_addr) {
      if (ptrace(PTRACE_SETREGS, tid, NULL, &old_regs)!=0){
         printf("[tracer][error] error in PTRACE_SETREGS\n");
         exit(-1);
      }
      if (ptrace(PTRACE_SETFPREGS, tid, NULL, &fregs)!=0){
         printf("[tracer][error] error in PTRACE_SETFPREGS\n");
         exit(-1);
      }
		
      #ifdef DEBUG_INFO	
      printf("---------------------------------\n");
      #endif
      return false;
   }
   return true;
}



void ptrace_cont(pid_t tid, struct user_regs_struct &regs, struct user_regs_struct &old_regs, struct user_fpregs_struct &fregs){
   printf("[tracer] after a PTRACE_SINGLESTEP, do a PTRACE_CONT\n");
   int status;
   ptrace(PTRACE_CONT, tid, NULL, NULL);
   int threadid = waitpid(tid, &status, 0);

   if (!WIFSTOPPED(status)){
      printf("[tracer][error] thread %d delivers a non-SIGSTOP signal\n", threadid);
      if (WIFEXITED(status)) {
         int num = WEXITSTATUS(status);
         printf("[tracer][error] tracee exits normally, exit num = %d\n", num);
      }
      if (WIFSIGNALED(status)){
         int num = WTERMSIG(status);
         printf("[tracer][error] tracee exits by signal, sig num = %d\n", num);
      }
      exit(-1);   
   }
   else {
      int sig = WSTOPSIG(status);	
      char *sig_str = strdup(_sys_siglist[sig]);
      if (sig != SIGSTOP){
         printf("[tracer][error] after PTRACE_CONT, thread %d delivers a non-SIGSTOP signal: %s\n", threadid, sig_str);
         if (ptrace(PTRACE_SETREGS, tid, NULL, &old_regs)!=0){
            printf("[tracer][error] error in PTRACE_SETREGS\n");
            exit(-1);
         }
         if (ptrace(PTRACE_SETFPREGS, tid, NULL, &fregs)!=0){
            printf("[tracer][error] error in PTRACE_SETFPREGS\n");
            exit(-1);
         }
         return;
      }
      ptrace(PTRACE_GETREGS, tid, NULL, &regs);

      #ifdef DEBUG_INFO
      printf("[tracer] after PTRACE_CONT, tracee delivers a signal %s\n", sig_str);
      printf("[tracer] RIP = %llx\n", regs.rip);
      #endif
   }
   if (ptrace(PTRACE_SETREGS, tid, NULL, &old_regs)!=0){
      printf("[tracer][error] error in setting registers of the target process\n");
      exit(-1);
   }
   if (ptrace(PTRACE_SETFPREGS, tid, NULL, &fregs)!=0){
      printf("[tracer][error] error in setting floating point registers of the target process\n");
      exit(-1);
   }

   printf("[tracer] machine code insertion finishes!\n");
   return;
}

