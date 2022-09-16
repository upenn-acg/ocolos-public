#include "utils.hpp"

using namespace std;

/*
 * Pause all threads of the currently running process by 
 * ptrace(PTRACE_ATTACH); and returns a list of tids that are 
 * stopped by ptrace.  
 */
vector<pid_t> pause_and_get_tids(pid_t target_pid);




/*
 * Leverage libunwind + ptrace functionality to get the functions 
 * that reside on the call stack, and return a list of address of 
 * their caller's call instructions that invoke them. 
 */
vector<unw_word_t> unwind_call_stack(vector<pid_t> tids);




/*
 * After ptrace stops the target process, change the ip points to 
 * the ld_preload code. Then perform a single-step execution on one 
 * thread of the target process. 
 *
 * This needs to be done because only in this way can we know 
 * whether the thread is blocked by the OS. If the thread is block 
 * by the OS, it cannot make further progress even if ptrace resumes
 * it. However, since we want to pick a thread that can execute our 
 * ld_preload lib code, we must pick a thread that is not blocked 
 * by the OS.
 */
bool ptrace_single_step(pid_t tid, void* lib_addr, struct user_regs_struct &regs, struct user_regs_struct &old_regs, struct user_fpregs_struct &fregs);




/*
 * After we pick the first thread that is not blocked by the OS,
 * we resume the thread by ptrace(PTRACE_CONT) to execute the 
 * ld_preload lib code for code replacement. 
 */
void ptrace_cont(pid_t tid, struct user_regs_struct &regs, struct user_regs_struct &old_regs, struct user_fpregs_struct &fregs);
