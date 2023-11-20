extern crate libc;
extern crate redhook;
extern crate core_affinity;

use std::ffi::{CStr,CString};
use std::time::SystemTime;
use libc::{size_t,ssize_t,c_char,c_int};
use std::{env,process};
use std::net::{TcpStream};
use std::str::from_utf8;
use std::io::{Read, Write, BufReader, BufRead, Error};

// store the clang path into String varc
const CLANG_PATH: &str = "/home.local/zyuxuan/clang/llvm-project/build/bin/clang";
const CLANGPP_PATH: &str = "/home.local/zyuxuan/clang/llvm-project/build/bin/clang++";
const CLANG12_PATH: &str = "/home.local/zyuxuan/clang/llvm-project/build/bin/clang-12";
const PERF_PATH: &str = "/usr/bin/perf";
const CLANG_BOLT_PATH: &str = "/home.local/zyuxuan/clang/llvm-project/build/bin/clang.bolt";
const PERF_DATA_DIR: &str = "/home.local/zyuxuan/tmp_rust/";

/* 
   helper functions to generate new pathname 
   as the pathname of the new execve call          
 */
fn change_pathname_to_perf()->(CString, *const c_char) {
  let pathname_perf = CString::new(PERF_PATH).expect("CString::new failed");
  let pathname_perf_c: *const c_char = pathname_perf.as_ptr();
  (pathname_perf, pathname_perf_c)
}

fn change_pathname_to_BOLTed_bin()->(CString, *const c_char) {
  let pathname_BOLTed_bin = CString::new(CLANG_BOLT_PATH).expect("CString::new failed");
  let pathname_BOLTed_bin_c: *const c_char = pathname_BOLTed_bin.as_ptr();
  (pathname_BOLTed_bin, pathname_BOLTed_bin_c)
}



/* 
   helper functions for argv[] manipulation:
   (1) [ wrap_charss ]: to wrap a char** into a Vec<CString> 
   (2) [ add_perf_prefix ]: add "perf record -e cycles:u -j 
       any,u -o perf{timestamp}.data -- " to a Vec<CString>
       and yeilds a new Vec<CString> 
   (3) [ wrap_argv_for_perf ]: generate a new argv for perf 
       based on the old argv[].
   (4) [ wrap_argv_for_bolted_bin ]: generated a new argv 
       for clang.bolt based on the old argv[].
 */
fn wrap_charss (css: *const *const c_char, v: &mut Vec<CString>) {
  let mut ptr = css;
  while unsafe { !(*ptr).is_null() } {
    let cs = unsafe{ CStr::from_ptr(*ptr) };
    v.push(cs.to_owned()); // make a copy of the env var
    ptr =  unsafe{ ptr.add(1) };
  }
}

fn add_perf_prefix (vcs: &mut Vec<CString>, cs: &mut Vec<CString>) {
  // generate time stamp
  let time_in_s = match SystemTime::now().duration_since(SystemTime::UNIX_EPOCH) {
    Ok(n) =>  n.as_millis().to_string(),
    Err(_) => panic!("SystemTime before UNIX EPOCH!"),
  };
  let mut perf: String = PERF_DATA_DIR.to_owned();
  perf.push_str("perf");
  perf.push_str(&time_in_s);
  perf.push_str(".data");
  cs.push(CString::new(PERF_PATH).expect("CString::new failed"));
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

fn wrap_argv_for_perf(argv: *const *const c_char)-> (std::boxed::Box<[std::ffi::CString]>, Vec<*const c_char>, *const *const c_char) {
  // convert argv into vector of CString
  let mut argv_cstring_: Vec<CString> = Vec::new();
  wrap_charss(argv, &mut argv_cstring_);

  // add "perf record ..." prefix to the argv_cstring_
  let mut argv_cstring : Vec<CString> = Vec::new();
  add_perf_prefix(&mut argv_cstring_, &mut argv_cstring);

  // convert Vec<CString> back to *const *const c_char
  let argv_vars = argv_cstring.into_boxed_slice();
  let mut argv_pointers = argv_vars.iter().map(|cs| cs.as_ptr() as *const c_char).collect::<Vec<*const c_char>>();
  argv_pointers.push(std::ptr::null()); // NULL sentinel at the end
  let argv_new = argv_pointers.as_ptr();

  (argv_vars, argv_pointers, argv_new)
}
 
fn wrap_argv_for_bolted_bin(argv: *const *const c_char)-> (std::boxed::Box<[std::ffi::CString]>, Vec<*const c_char>, *const *const c_char) {
   // convert argv into vector of CString
   let mut argv_cstring: Vec<CString> = Vec::new();
   wrap_charss(argv, &mut argv_cstring);

   // change the first element in argv[] to the path to bolted clang
   argv_cstring[0] = CString::new(CLANG_BOLT_PATH).expect("CString::new failed");
   let argv_vars = argv_cstring.into_boxed_slice();

   // convert Vec<CString> back to *const *const c_char
   let mut argv_pointers = argv_vars.iter().map(|cs| cs.as_ptr() as *const c_char).collect::<Vec<*const c_char>>();
   argv_pointers.push(std::ptr::null()); // NULL sentinel at the end
   let argv_new = argv_pointers.as_ptr();
   (argv_vars, argv_pointers, argv_new)
}



/*
   helper functions for envp[] manipulation:
   (1) [ get_LD_PRELOAD_idx ]: get the index of "LD_PRELOAD" 
       from envp[].
   (2) [ wrap_envp ]: generated a new envp with the same 
       format of *const *const c_char envp, but have no 
       LD_PRELOAD as its environmental variable.
 */
fn get_LD_PRELOAD_idx (vcs: &mut Vec<CString>) -> usize{
  let mut idx: usize = 0;
  for item in vcs {
    let cs: String = item.to_str().unwrap().to_owned();
    if cs.len()>10 && (&cs[0..10]).eq("LD_PRELOAD") {
      break;
    }
    idx = idx + 1;
  }
  idx
}


fn wrap_envp(envp: *const *const c_char)-> (std::boxed::Box<[std::ffi::CString]>, Vec<*const c_char>, *const *const c_char) {
  let mut envp_cstring: Vec<CString> = Vec::new();
  wrap_charss(envp, &mut envp_cstring);

  // delete LD_PRELOAD from envp
  let index = get_LD_PRELOAD_idx(&mut envp_cstring);
  envp_cstring.remove(index);

  // convert Vec<CString> back to the *const *const c_char
  let envp_vars = envp_cstring.into_boxed_slice();
  let mut envp_pointers = envp_vars.iter().map(|cs| cs.as_ptr() as *const c_char).collect::<Vec<*const c_char>>();
  envp_pointers.push(std::ptr::null()); // NULL sentinel at the end
  let envp_new = envp_pointers.as_ptr();
  (envp_vars, envp_pointers, envp_new)
}






redhook::hook! {
    // int execve(const char *pathname, char *const argv[], char *const envp[])
    unsafe fn execve(pathname: *const c_char, argv: *const *const c_char, envp: *const *const c_char) -> c_int => my_execve {
        // compare if there is a match between clang_path 
        // and the pathname of the calling function execve().
        let pathname_string: &str = CStr::from_ptr(pathname).to_str().unwrap();
        if pathname_string.eq(CLANG_PATH) ||  
           pathname_string.eq(CLANGPP_PATH) || 
           pathname_string.eq(CLANG12_PATH) {

          /* Since clang is invoked, we connect to the server
             the protocol is very simple:
             (1)
             client ------"clang invocation"_----> server
             client <----"start perf profiling"--- server
             client profile using perf.

             (2)
             client ------"clang invocation"_----> server
             client <-"run program without perf"-- server
             client directly run the program.

             (3)
             client ------"clang invocation"_----> server
             client <----"run bolted binary"------ server
             client directly run the bolted binary.
         */  
        
          match TcpStream::connect("localhost:8000") {
            Ok(mut stream) => {
              println!("Successfully connected to server in port 8000");
              
              /* first send a "clang invocation" message 
                 to the server. and then create a buffer
                 to receive response from server, then 
                 check the response.
               */ 
              let msg = b"clang invocation";
              stream.write(msg).unwrap();
              let mut buf = [0 as u8; 64]; 

              match stream.read(&mut buf) {
                Ok(_size) => {
                  if &buf[0..20] == b"start perf profiling" {

                    /* (1) change the path name to the path of Linux Perf
                       (2) convert argv into argv_new, which adds 
                           "perf record -e cycles:u -j any,u -o perf{timestamp}.data -- "
                           before the argv.
                       (3) convert envp into envp_new, which deletes
                           LD_PRELOAD environmental variable from 
                           envp.
                     */

                    let (pathname_perf, pathname_perf_c) = change_pathname_to_perf();
                    let (argv_vars, argv_pointers, argv_new) = wrap_argv_for_perf(argv);
                    let (envp_vars, envp_pointers, envp_new) = wrap_envp(envp);

                    // replace the old execve call of clang with our new execve.
                    redhook::real!(execve)(pathname_perf_c, argv_new, envp_new)
                  }
                  else if &buf[0..17] == b"run bolted binary" {
                    let text = from_utf8(&buf).unwrap();
                    let (path_name, pathname_perf_c): (CString, *const c_char) = change_pathname_to_BOLTed_bin();
                    let (argv_vars, argv_pointers, argv_new) = wrap_argv_for_bolted_bin(argv); 
                    let (envp_vars, envp_pointers, envp_new) = wrap_envp(envp);
                    redhook::real!(execve)(pathname_perf_c, argv_new, envp_new)
                  }
                  else {
                    let text = from_utf8(&buf).unwrap();
                    println!("Unexpected reply: {}", text);
                    redhook::real!(execve)(pathname, argv, envp)
                  }
                },
                Err(e) => {
                  println!("Failed to receive data: {}", e);
                  redhook::real!(execve)(pathname, argv, envp)
                }
              }
            },
            Err(e) => {
              println!("Failed to connect: {}", e);
              redhook::real!(execve)(pathname, argv, envp)
            }
          }
        }
        else{
          redhook::real!(execve)(pathname, argv, envp)
        }
    }
}



redhook::hook! {
    // int execvp(const char *pathname, char *const argv[])
    unsafe fn execvp(pathname: *const c_char, argv: *const *const c_char) -> c_int => my_execvp {

        let pathname_string: &str = CStr::from_ptr(pathname).to_str().unwrap();
        
        if pathname_string.eq(CLANG_PATH) || 
           pathname_string.eq(CLANGPP_PATH) || 
           pathname_string.eq(CLANG12_PATH) {
  
          let (pathname_perf, pathname_perf_c) = change_pathname_to_perf();
          let (argv_vars, argv_pointers, argv_new) = wrap_argv_for_perf(argv);

          // delete the env variable "LD_PRELOAD"
          let key = "LD_PRELOAD";
          env::remove_var(key);
          assert!(env::var(key).is_err()); 

          redhook::real!(execvp)(pathname_perf_c, argv_new)  
        }
        else{
          redhook::real!(execvp)(pathname, argv)
        }
    } 
} 
