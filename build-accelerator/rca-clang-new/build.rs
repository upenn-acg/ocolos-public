extern crate gcc;

fn main() {
    gcc::Config::new()
                .file("src/c/c_functions.c")
                .include("src")
                .compile("libc_functions.a");
}

