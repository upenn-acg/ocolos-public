(1) in src/lib.rs, please modify line 17 ~ line 21.
    "perf_data_dir" is the directory where you store all of your perf.data.

(2) type "cargo build" in terminal to build the library. 
    The built library "librca.so" is in target/debug.

(3) here is a simple example to add this library:
    suppose we want to run "clang main.c -o main", we should run it in this way: 
    LD_PRELOAD={your path}/floar/rca-clang-new/target/debug/librca.so /bin/sh -c "{your path}/llvm-project/build/bin/clang main.c -o main"  
    (you should replace {your path} to your real path for "floar" and "clang")

(4) the profiled result will be stored in the dir you specified in "perf_data_dir" in lib.rs  
