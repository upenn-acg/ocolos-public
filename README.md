# Ocolos: Online COde Layout OptimizationS
Ocolos is the first _online_ code layout optimization system for unmodified applications written in unmanaged languages. Ocolos allows profile-guided optimization to be performed on a running process, instead of being performed offline and requiring the application to be re-launched. A description of how we implemented Ocolos and experimental results on MySQL-sysbench workloads are in [MICRO'22 paper](https://ieeexplore.ieee.org/document/9923868).

For the demonstration purpose, we integrate `MySQL` and `sysbench` to Ocolos, so this version of Ocolos ONLY works with `MySQL`. 




## Prerequisites
Please refer instructions from links or directly run commands listed below to install prerequisites: 
- Clang(for building MySQL): [https://clang.llvm.org/get_started.html](https://clang.llvm.org/get_started.html) 
- Linux Perf:`sudo apt-get install linux-tools-common linux-tools-generic` 
- libunwind: [https://github.com/libunwind/libunwind](https://github.com/libunwind/libunwind) 
- Rust: `curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh` 
- boost_1_73_0 (for MySQL): [https://www.boost.org/users/history/version_1_73_0.html](https://www.boost.org/users/history/version_1_73_0.html) 
- binutils (for objdump) [^1] : [https://ftp.gnu.org/gnu/binutils/](https://ftp.gnu.org/gnu/binutils/) 
- boost: `sudo apt-get install libboost-dev` (for Ubuntu)
[^1]:If your `objdump` version is older than 2.27, please download the latest version of `binutils`.  

## Download ocolos for mysql
```
$ git clone git@github.com:upenn-acg/Ocolos-MySQL.git 
```


## Install BOLT 
To use `llvm-bolt` and `perf2bolt` utilities, `BOLT` needs to be installed. \
Please follow the commands below to install `BOLT` 
```bash
> git clone https://github.com/facebookincubator/BOLT llvm-bolt
> cd llvm-bolt
> git checkout 88c70afe9d388ad430cc150cc158641701397f70
> cp {path to ocolos_mysql directory}/BOLT_replace/RewriteInstance.cpp bolt/lib/Rewrite/RewriteInstance.cpp
> cp {path to ocolos_mysql directory}/BOLT_replace/BinaryContext.cpp bolt/lib/Core/BinaryContext.cpp
> cd .. && mkdir build && cd build
> cmake -G Ninja ../llvm-bolt/llvm -DLLVM_TARGETS_TO_BUILD="X86;AArch64" -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_ASSERTIONS=ON -DLLVM_ENABLE_PROJECTS="clang;lld;bolt"
> ninja
```


## Build MySQL from source 
```bash
> git clone https://github.com/mysql/mysql-server.git 
> cd mysql-server 
> git checkout 6846e6b2f72931991cc9fd589dc9946ea2ab58c9 
```
In `CMakeList.txt`, at line 582, please add: \
`STRING_APPEND(CMAKE_C_FLAGS  " -fno-jump-tables")` \
`STRING_APPEND(CMAKE_CXX_FLAGS " -fno-jump-tables")` 

Also, in `CMakeList.txt`, turn off `ld.gold` linker: \
change `OPTION(USE_LD_GOLD "Use GNU gold linker" ON)` to be `OPTION(USE_LD_GOLD "Use GNU gold linker" OFF)`

Then build `mysqld`[^2] from source:
```bash
> export CC={the path of the Clang binary} 
> export CXX={the path of the Clang++ binary (actually symbolic link)} 
> mkdir build && cd build 
> cmake .. -DWITH_BOOST={path of the boost_1_73_0 directory} -DCMAKE_CXX_LINK_FLAGS=-Wl,--emit-relocs -DCMAKE_C_LINK_FLAGS=-Wl,--emit-relocs -DBUILD_CONFIG=mysql_release 
> make -j
> make install
```
[^2]: The target binary must to be compiled by `Clang` rather than `gcc`. A `gcc`-compiled binary will not work with `Ocolos`.

To initialize MySQL, run:
```bash
> chown {user}:{user} {path to MySQL directory}
> {path to MySQL directory}/bin/mysqld --initialize-insecure --user=root --datadir={your data dir path of MySQL} 
> {path to MySQL directory}/bin/mysqld --user=root --port=3306 --datadir={your data dir path of MySQL}
```
In another terminal, run:
```bash
> mysql -u root
> CREATE USER 'ocolos'@'localhost';
> GRANT ALL PRIVILEGES ON *.* TO 'ocolos'@'localhost' WITH GRANT OPTION;
> CREATE DATABASE ocolos_db;
> QUIT;
> mysqladmin -u root shutdown
```
_Note_: 
1. {path to MySQL directory} is normally `/usr/local/mysql` unless otherwise specified during MySQL server's installation.  
2. {user} should be your linux user name. 


## Build Sysbench 
Please refer instructions in the following webpage:\
[https://github.com/akopytov/sysbench](https://github.com/akopytov/sysbench) 


## Build & run Ocolos
- Navigate to `ocolos_mysql` directory.  
- In the file `config`, specify the absolute path for `nm`,`perf`,`objdump`,`llvm-bolt`,`perf2bolt` [^3]
   [^3]: if `nm`,`objdump` and `perf` are already in shell, it's OK that their paths are not specified in `config`. This can be checked by `which nm`, `which objdump` and `which perf`.
- In `config`, please also specify the commands to run `MySQL server` and `sysbench`. The example commands are given in the config file. 
   * _Note_: the first argument of the command (a.k.a. the binary being invoked in the command) should be written in its full path. 
- Then run the following commands:

```bash
> make
> ./extract_call_sites
> ./tracer

```
- `make` will produce 2 executables (`tracer` & `extract_call_sites`)+ 1 shared library (`replace_function.so`). 
   * If libunwind library is stored in other places instead of `/usr/local/lib`, you also need to edit Makefile and update it to the corresponding path.
   * If libunwind's header files are stored in other places instead of `/usr/local/include`, you also need to edit Makefile and update it to the corresponding path. 
- `./extract_call_sites` will produce to 2 files which store all call sites information extracted from the target binary (a.k.a. `mysqld`) to the `tmp_data_dir` you specified in the config file. 
- `./tracer` will invoke both `MySQL` server process and sysbench workloads `oltp_read_only`, and then perform code layout optimization during runtime. 
   * The output of sysbench's throughput can be found in `sysbench_output.txt`. At about the 130th second, you will see a significant throughput improvement, since Ocolos has replace the code layout to be the optimized one at that time.
   * After one run (~3 minutes), if you want to start another run, please first run `mysqladmin -u root shutdown` command to shutdown the current `MySQL` server process. 
 
## Miscellaneous
In `src/utils.hpp`,
- if `TIME_MEASUREMENT` is defined, Ocolos will print the execution time of code replacement;
- if `MEASUREMENT` is defined, Ocolos will print metrics such as:  
  * the number of functions on the call stack when target process is paused,
  * the number of functions that are moved by BOLT,
  * the number of functions that are in the BOLT and original functions.
- if `DEBUG_INFO` is defined, Ocolos will print debug information such as:
  * the information about detailed behavior of tracer
  * the content in the call stack when the target process is paused
  * `DEBUG_INFO` can also be defined in `src/replace_function.hpp`. In this way, the ld_preload library can store all machine code per function it inserted to the target process as a `uint8_t` format array into a file. The file can be found in the `tmp_data_path` you defined in the config file. 
- if `DEBUG` is defined, after code replacement, Ocolos will first send `SIGSTOP` signal to target process and then resume the target process by `PTRACE_DETACH`. In this way, it allows debugging tools such as `GDB` to attach to the target process and observe what goes wrong after code replacement.


