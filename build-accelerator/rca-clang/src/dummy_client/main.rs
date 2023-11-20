use std::env;
use std::process;
use std::fs::File;
use std::io::{Read, Write, BufReader, BufRead, Error};
//use std::path::Path;
//use std::ffi::OsStr;
use std::net::{TcpStream};
use std::str::from_utf8;

fn main() {
  match TcpStream::connect("localhost:8000") {
    Ok(mut stream) => {
      println!("Successfully connected to server in port 8000");
      let msg = b"clang invocation";
      stream.write(msg).unwrap();
      println!("Sent Hello, awaiting reply...");

      let mut data = [0 as u8; 64]; 
      match stream.read(&mut data) {
        Ok(_) => {
          if &data[0..20] == b"start perf profiling" {
            println!("Reply is ok!");
          } 
          else {
            let text = from_utf8(&data).unwrap();
            println!("Unexpected reply: {}", text);
          }
        },
        Err(e) => {
          println!("Failed to receive data: {}", e);
        }
      }
    },
    Err(e) => {
      println!("Failed to connect: {}", e);
    }
  }
  println!("Terminated.");
}

