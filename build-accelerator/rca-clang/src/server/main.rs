extern crate core_affinity;

use std::{process, thread, str, fs};
use std::io::{Read, Write};
use std::net::{TcpListener, TcpStream, Shutdown};
use std::sync::{Arc, Mutex};
use std::time::{Duration, SystemTime};

const PERF_DATA_PATH: &str = "/home.local/zyuxuan/tmp_rust/";
const CLANG12_PATH: &str = "/home.local/zyuxuan/clang/llvm-project/build/bin/clang-12";
const CLANG_BOLT_PATH: &str = "/home.local/zyuxuan/clang/llvm-project/build/bin/clang.bolt";
const NUM_PERF_DATA: i32 = 10;
const NUM_PERF2BOLT_IN_PARALLEL: usize = 4;

fn client_run_perf(mut stream: TcpStream) {
  let mut data = [0 as u8; 50]; // using 50 byte buffer
  match stream.read(&mut data) {
    Ok(_size) => {
      
      let data_str = str::from_utf8(&data).unwrap();
      //println!("The received message is \"{}\"", data_str);
      if &data[0..16]==b"clang invocation" {
        thread::sleep_ms(3000);
        stream.write(b"start perf profiling").unwrap();
      }
      else {
        stream.write(b"run program without perf").unwrap();
      }
      //stream.write(&data[0..size]).unwrap();
    },
    Err(_) => {
      //println!("An error occurred, terminating connection with {}", stream.peer_addr().unwrap());
      stream.shutdown(Shutdown::Both).unwrap();
    }
  }
}


fn client_not_run_perf(mut stream: TcpStream) {
  let mut data = [0 as u8; 50]; // using 50 byte buffer
  match stream.read(&mut data) {
    Ok(_size) => {
      let data_str = str::from_utf8(&data).unwrap();
      //println!("The received message is \"{}\"", data_str);
      stream.write(b"run program without perf").unwrap();
    },
    Err(_) => {
      //println!("An error occurred, terminating connection with {}", stream.peer_addr().unwrap());
      stream.shutdown(Shutdown::Both).unwrap();
    }
  }
}


fn client_run_bolted_binary(mut stream: TcpStream) {
  let mut data = [0 as u8; 50]; // using 50 byte buffer
  match stream.read(&mut data) {
    Ok(_size) => {
      let data_str = str::from_utf8(&data).unwrap();
      //println!("The received message is \"{}\"", data_str);
      stream.write(b"run bolted binary").unwrap();
    },
    Err(_) => {
      //println!("An error occurred, terminating connection with {}", stream.peer_addr().unwrap());
      stream.shutdown(Shutdown::Both).unwrap();
    }
  }
}


fn generate_perf_fdata() {
  let paths = fs::read_dir(PERF_DATA_PATH).unwrap();
  
  let mut children: Vec<std::process::Child> = Vec::new();

  for path in paths {
    let path1 = path.unwrap().path().into_os_string().into_string().unwrap();
    let length = path1.len();
    let mut path2 = (&path1[0..(length-4)]).to_owned();
    path2.push_str("fdata");

    // convert perf.data into perf.fdata
    let child = process::Command::new("perf2bolt")
      .arg("-p")
      .arg(&path1)
      .arg("-o")
      .arg(&path2)
      .arg(CLANG12_PATH)
      .spawn()
      .expect("failed to execute process");
    children.push(child);
    if children.len() == NUM_PERF2BOLT_IN_PARALLEL {
      for i in 0..NUM_PERF2BOLT_IN_PARALLEL {
        //let core_ids = core_affinity::get_core_ids().unwrap();
        //core_affinity::set_for_current(core_ids[i]);
        let ecode = children[i].wait().expect("failed to wait on child");
      }
      children.clear();
    }
  }
  for i in 0..(children.len()) {
    let ecode = children[i].wait().expect("failed to wait on child");
  }
  children.clear();
}

fn merge_perf_fdata(){
  // create the merge-fdata command 
  let mut arg2: String = String::from("merge-fdata ");
  arg2.push_str(PERF_DATA_PATH);
  arg2.push_str("*.fdata > perf.fdata");

  // run the merge-fdata command in shell
  let mut child = process::Command::new("sh")
      .arg("-c")
      .arg(&arg2)
      .spawn().expect("failed to execute process");
  let ecode1 = child.wait().expect("failed to wait on child");
}

fn generate_bolted_binary(){
  let mut child2 = process::Command::new("llvm-bolt")
    .arg(CLANG12_PATH)
    .arg("-o")
    .arg(CLANG_BOLT_PATH)
    .arg("-data=perf.fdata")
    .arg("-reorder-blocks=cache+")
    .arg("-reorder-functions=hfsort")
    .arg("-split-functions=2")
    .arg("-split-all-cold")
    .arg("-split-eh")
    .arg("-dyno-stats")
    .spawn()
    .expect("failed to execute process");
  let ecode2 = child2.wait().expect("failed to wait on child");
}

fn time_measurement(sys_time: std::time::SystemTime){
  let difference = SystemTime::now().duration_since(sys_time)
                                    .expect("Clock may have gone backwards");
  println!("@@@ current time from the server setup: {:?}", difference);        
}


fn main() {
  let listener = TcpListener::bind("127.0.0.1:8000").unwrap();
  // accept connections and process them, spawning a new thread for each one
  println!("Server listening on port 8000");
  let mutex = Arc::new(Mutex::new(false));
  let mut count: i32 = 0;
  let sys_time = SystemTime::now();

  for stream in listener.incoming() {
    match stream {
      Ok(stream) => {
        //println!("New connection: {}", stream.peer_addr().unwrap());
         
        let mutex = Arc::clone(&mutex);
        let mut bolt_binary_generated = false;

        { let tmp = mutex.lock().unwrap();
          bolt_binary_generated = *tmp; 
        }
        count += 1;

        if count == 1 {
          time_measurement(sys_time);
        }

        if bolt_binary_generated == true {
          client_run_bolted_binary(stream);
        }
        else { 
          if count <= NUM_PERF_DATA {
            client_run_perf(stream);
          }
          else {
            client_not_run_perf(stream);
          }

          if count == ( NUM_PERF_DATA + 1) {
            thread::spawn(move|| {
              thread::sleep_ms(2000);
              time_measurement(sys_time);
              generate_perf_fdata();
              time_measurement(sys_time);
              merge_perf_fdata();
              time_measurement(sys_time);
              generate_bolted_binary();
              let mutex = Arc::clone(&mutex);
              let mut tmp = mutex.lock().unwrap();
              *tmp = true;
              time_measurement(sys_time);
            });
          }
        }
      }
      Err(e) => {
        println!("Error: {}", e);
        /* connection failed */
      }
    }
  }
  // close the socket server
  drop(listener);
}

