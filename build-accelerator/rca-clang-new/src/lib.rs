extern crate libc;
extern crate redhook;

use std::ffi::CStr;
use std::time::SystemTime;
use libc::{size_t,ssize_t,c_char,c_int};
use std::ffi::CString;
use std::env;

#[link(name="c_functions", kind="static")]
extern { 
  fn convert_path(argv: *const *const c_char, args: *const *const c_char) -> c_int;
  fn convert_envp(envp: *const *const c_char, envp_new: *const *const c_char) -> c_int;
}

// store the clang path into String var
const clang_path: &str = "/home.local/zyuxuan/clang/llvm-project/build/bin/clang";
const clangpp_path: &str = "/home.local/zyuxuan/clang/llvm-project/build/bin/clang++";
const clang12_path: &str = "/home.local/zyuxuan/clang/llvm-project/build/bin/clang-12";
const perf_data_dir: &str = "/home.local/zyuxuan/tmp_rust/perf";
const perf_path: &str = "/usr/bin/perf";


// helper function to add "perf record ..." prefix before the real command 
fn add_perf_prefix (vcs: &mut Vec<CString>, cs: &mut Vec<CString>) {
  // generate time stamp
  let time_in_s = match SystemTime::now().duration_since(SystemTime::UNIX_EPOCH) {
    Ok(n) =>  n.as_millis().to_string(),
    Err(_) => panic!("SystemTime before UNIX EPOCH!"),
  };
  let mut perf: String = perf_data_dir.to_owned();
    perf.push_str(&time_in_s);
    perf.push_str(".data");
    cs.push(CString::new(perf_path).expect("CString::new failed"));
    cs.push(CString::new("record").expect("CString::new failed"));
    cs.push(CString::new("-e").expect("CString::new failed"));
    cs.push(CString::new("cycles:u").expect("CString::new failed"));
    cs.push(CString::new("-j").expect("CString::new failed"));
    cs.push(CString::new("any,u").expect("CString::new failed"));
    cs.push(CString::new("-o").expect("CString::new failed"));
    cs.push(CString::new(perf).expect("CString::new failed"));
    cs.push(CString::new("--").expect("CString::new failed"));
    for item in vcs {
      cs.push(item.to_owned());
    }
}

redhook::hook! {
    // int execve(const char *pathname, char *const argv[], char *const envp[])
    unsafe fn execve(pathname: *const c_char, argv: *const *const c_char, envp: *const *const c_char) -> c_int => my_execve {

        // convert c_char argument "pathname" into Rust CString
        // for a later comparison.
        let pathname_cstr: &CStr = CStr::from_ptr(pathname);
        let pathname_string: &str = pathname_cstr.to_str().unwrap();

        // compare if there is a match between clang_path 
        // and the path of the calling function.
        if pathname_string.eq(clang_path) || 
           pathname_string.eq(clangpp_path) || 
           pathname_string.eq(clang12_path) {
          //println!("clang is called...");

          // change path name to the path of Linux Perf.
          let pathname_perf = CString::new(perf_path).expect("CString::new failed");
          let pathname_perf_c: *const c_char = pathname_perf.as_ptr();

          // helper function to wrap a char** into a Vec<CString> 
          // for a more rustic experience
          let wrap_charss = |css: *const *const c_char, v: &mut Vec<CString>| {
            let mut ptr = css;
            while unsafe { !(*ptr).is_null() } {
              let cs = CStr::from_ptr(*ptr) ;
              v.push(cs.to_owned()); // make a copy of the env var
              ptr =  ptr.add(1) ;
            }
          };

          // helper function to get the index of "LD_PRELOAD" from envp 
          let get_LD_PRELOAD_idx = |vcs: &mut Vec<CString> | -> usize{
            let mut idx: usize = 0;
            for item in vcs {
              let cs: String = item.to_str().unwrap().to_owned();
              if cs.len()>10 && (&cs[0..10]).eq("LD_PRELOAD") {
                break;
              }
              idx = idx + 1;
            }
            idx
          };

          // convert argv into vector of CString
          let mut argv_cstring_: Vec<CString> = Vec::new();
          wrap_charss(argv, &mut argv_cstring_);
          // add "perf record ..." prefix to the argv_cstring_
          let mut argv_cstring : Vec<CString> = Vec::new();
          add_perf_prefix(&mut argv_cstring_, &mut argv_cstring);

          // convert envp into vector of CString
          let mut envp_cstring: Vec<CString> = Vec::new();
          wrap_charss(envp, &mut envp_cstring);
          // delete LD_PRELOAD from envp
          let index = get_LD_PRELOAD_idx(&mut envp_cstring);
          envp_cstring.remove(index);

          // convert envp_cstring & argv_cstring from 
          // Vec<CString> to pointers that points to 
          // Vec<CString> 
          let envp_vars = envp_cstring.into_boxed_slice();
          let mut envp_pointers = envp_vars.iter().map(|cs| cs.as_ptr() as *const c_char).collect::<Vec<*const c_char>>();
          envp_pointers.push(std::ptr::null()); // NULL sentinel at the end
          let envp_new = envp_pointers.as_ptr();

          let argv_vars = argv_cstring.into_boxed_slice();
          let mut argv_pointers = argv_vars.iter().map(|cs| cs.as_ptr() as *const c_char).collect::<Vec<*const c_char>>();
          argv_pointers.push(std::ptr::null()); // NULL sentinel at the end
          let argv_new = argv_pointers.as_ptr();

          // replace the old execve call of clang with our new execve.
          redhook::real!(execve)(pathname_perf_c, argv_new, envp_new)
        }
        else{
          redhook::real!(execve)(pathname, argv, envp)
        }
    }
}



redhook::hook! {
    // int execvp(const char *pathname, char *const argv[])
    unsafe fn execvp(pathname: *const c_char, argv: *const *const c_char) -> c_int => my_execvp {

        // convert c_char argument "pathname" into Rust CString
        // for a later comparison.
        let pathname_cstr: &CStr = CStr::from_ptr(pathname);
        let pathname_string: &str = pathname_cstr.to_str().unwrap();

        // compare if there is a match between clang_path 
        // and the path of the calling function.
        if pathname_string.eq(clang_path) || 
           pathname_string.eq(clangpp_path) || 
           pathname_string.eq(clang12_path) {

          // change path name into the path of Linux Perf.
          let pathname_perf = CString::new(perf_path).expect("CString::new failed");
          let pathname_perf_c: *const c_char = pathname_perf.as_ptr();

          // helper function to wrap a char** into a Vec<CString> for a more rustic experience
          let wrap_charss = |css: *const *const c_char, v: &mut Vec<CString>| {
            let mut ptr = css;
            while unsafe { !(*ptr).is_null() } {
              let cs = CStr::from_ptr(*ptr) ;
              v.push(cs.to_owned()); // make a copy of the env var
              ptr =  ptr.add(1) ;
            }
          };

          // convert argv into vector of CString
          let mut argv_cstring_: Vec<CString> = Vec::new();
          wrap_charss(argv, &mut argv_cstring_);
          // add "perf record ..." prefix to the argv_cstring_
          let mut argv_cstring : Vec<CString> = Vec::new();
          add_perf_prefix(&mut argv_cstring_, &mut argv_cstring);

          // convert argv_cstring from Vec<CString> to pointer 
          // points to Vec<CString> of argv 
          let argv_vars = argv_cstring.into_boxed_slice();
          let mut argv_pointers = argv_vars.iter().map(|cs| cs.as_ptr() as *const c_char).collect::<Vec<*const c_char>>();
          argv_pointers.push(std::ptr::null()); // NULL sentinel at the end
          let argv_new = argv_pointers.as_ptr();

          // delete the env variable "LD_PRELOAD"
          let key = "LD_PRELOAD";
          env::remove_var(key);
          assert!(env::var(key).is_err()); 

          // replace the old execve call of clang with our new execve.
          redhook::real!(execvp)(pathname_perf_c, argv_new)  
        }
        else{
          redhook::real!(execvp)(pathname, argv)
        }
    } 
} 
