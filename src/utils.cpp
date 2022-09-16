#include "utils.hpp"
using namespace std;


ocolos_env::ocolos_env(){
   ocolos_env::read_config();
   get_dir_path();
   get_cmds();

   ocolos_env::bolted_binary_path   = ocolos_env::tmp_data_path+"mysqld.bolt";
   ocolos_env::ld_preload_path      = ocolos_env::lib_path+"replace_function.so";
   ocolos_env::bolted_function_bin  = ocolos_env::tmp_data_path+"bolted_functions.bin";
   ocolos_env::call_sites_bin       = ocolos_env::tmp_data_path+"call_sites.bin";
   ocolos_env::v_table_bin          = ocolos_env::tmp_data_path+"v_table.bin";
   ocolos_env::unmoved_func_bin     = ocolos_env::tmp_data_path+"unmoved_func.bin";
   ocolos_env::call_sites_all_bin   = ocolos_env::tmp_data_path+"call_sites_all.bin";
   ocolos_env::call_sites_list_bin  = ocolos_env::tmp_data_path+"call_sites_list.bin";

   ocolos_env::perf_path            = ocolos_env::get_binary_path("perf");
   ocolos_env::nm_path              = ocolos_env::get_binary_path("nm");
   ocolos_env::objdump_path         = ocolos_env::get_binary_path("objdump");

   ocolos_env::llvmbolt_path        = ocolos_env::get_binary_path("llvm-bolt");
   ocolos_env::perf2bolt_path       = ocolos_env::get_binary_path("perf2bolt");
   
   ocolos_env::client_binary_path   = ocolos_env::get_binary_path("mysql");

   ocolos_env::listen_fd            = socket(PF_INET, SOCK_STREAM, 0);

}




void ocolos_env::read_config(){
   FILE* config = fopen("config","r");
   char line[4096];
   while (fgets (line , 4096 , config) != NULL){
      string current_line(line);
      if (current_line[current_line.length()-1]=='\n') current_line.pop_back();

      if ((current_line.length()>0)&&(current_line[0]!='#')){
         string delimiter = "=";
         string bin_name = current_line.substr(0, current_line.find(delimiter));
         string path_name = current_line.substr(current_line.find(delimiter), current_line.length());
         if ((path_name.length()>0)&&(path_name[0]=='=')){
            path_name.erase(path_name.begin());
            ocolos_env::configs[bin_name] = path_name;
         }
      }
   } 
}




string ocolos_env::get_binary_path(string bin_name){
   if (ocolos_env::configs.find(bin_name) != ocolos_env::configs.end()){
      return ocolos_env::configs[bin_name];
   }
   string cmd = "which "+bin_name;
   char* cmd_cstr = new char[cmd.length()+1];
   strcpy(cmd_cstr,cmd.c_str());
   FILE* fp = popen(cmd_cstr, "r");
   char path[4096];
   if (fp == NULL){
      printf("[tracer][error] Failed to run linux which command\n");
      exit(-1);
   }
   if (fgets(path, sizeof(path), fp)!=NULL){
      string path_str(path);
      if (path_str[path_str.length()-1]=='\n') path_str.pop_back();
      return path_str;
   }
   else{
      printf("[tracer][error] you didn't specify %s in config file, nor did %s exists in your system\n", bin_name.c_str(), bin_name.c_str());
      exit(-1);
   }
   return "";
}



void ocolos_env::get_dir_path(){
   char dir_path[1024];
   if(getcwd(dir_path,1024)==NULL) {
      printf("[tracer][error] error in getting the path of the current dir\n");
      exit(-1);
   }
   string dir_path_str(dir_path); 
   if (dir_path_str[dir_path_str.size()-1] != '/')
      dir_path_str.push_back('/');
   ocolos_env::dir_path = dir_path_str;

   ocolos_env::tmp_data_path = ocolos_env::get_binary_path("tmp_data_dir");
   if (ocolos_env::tmp_data_path[ocolos_env::tmp_data_path.length()-1] !='/'){
      ocolos_env::tmp_data_path.push_back('/');
   }
   ocolos_env::lib_path = ocolos_env::get_binary_path("lib");
   if (ocolos_env::lib_path[ocolos_env::lib_path.length()-1] !='/'){
      ocolos_env::lib_path.push_back('/');
   }
}




void ocolos_env::get_cmds(){
   if (ocolos_env::configs.find("server_cmd")==ocolos_env::configs.end()){
      printf("[tracer][error] error in reading `server_cmd` in config file.\n");
      exit(-1);
   }
   if (ocolos_env::configs.find("init_benchmark_cmd")==ocolos_env::configs.end()){
      printf("[tracer][error] error in reading `init_benchmark_cmd` in config file.\n");
      exit(-1);
   }
   if (ocolos_env::configs.find("run_benchmark_cmd")==ocolos_env::configs.end()){
      printf("[tracer][error] error in reading `run_benchmark_cmd` in config file.\n");
      exit(-1);
   }
   ocolos_env::run_server_cmd = ocolos_env::configs["server_cmd"];
   ocolos_env::init_benchmark_cmd = ocolos_env::configs["init_benchmark_cmd"];
   ocolos_env::run_benchmark_cmd = ocolos_env::configs["run_benchmark_cmd"]; 
   
   ocolos_env::target_binary_path = split_line(ocolos_env::run_server_cmd)[0];

   vector<string> run_benchmark_cmd_args = split_line(ocolos_env::run_benchmark_cmd);

   bool has_db = false;
   bool has_user = false;
   for (unsigned int i=0; i<run_benchmark_cmd_args.size(); i++){
      if (run_benchmark_cmd_args[i].length()>10){
         string first11 = run_benchmark_cmd_args[i].substr(0,11);
         if (first11=="--mysql-db="){
            ocolos_env::db_name = run_benchmark_cmd_args[i].substr(11, run_benchmark_cmd_args[i].length());
            if (ocolos_env::db_name[ocolos_env::db_name.length()-1]=='\n') ocolos_env::db_name.pop_back();
            has_db = true;
         }
      }
      if (run_benchmark_cmd_args[i].length()>12){
         string first13 = run_benchmark_cmd_args[i].substr(0,13);
         if (first13=="--mysql-user="){
            ocolos_env::db_user_name = run_benchmark_cmd_args[i].substr(13, run_benchmark_cmd_args[i].length());
            if (ocolos_env::db_user_name[ocolos_env::db_user_name.length()-1]=='\n') ocolos_env::db_user_name.pop_back();
            has_user = true;
         }
      }
   }

   if (has_db==false) {
      printf("[tracer][error] the benchmark command didn't specify db's name\n");
      exit(-1);
   }
   if (has_user==false) {
      printf("[tracer][error] the benchmark command didn't specify user's name\n");
      exit(-1);
   }
}





vector<string> split_line(string str){
   vector<string> words;
   stringstream ss(str);
   string tmp;
   while (ss >> tmp ){
      words.push_back(tmp);
      tmp.clear();
   }
   return words;
}


vector<string> split (const string &s, char delim) {
   vector<string> result;
   stringstream ss (s);
   string item;

   while (getline (ss, item, delim)) {
      result.push_back (item);
   }
   return result;
}


long convert_str_2_int(string str){
   long result = 0;
   for (unsigned i=0; i<str.size(); i++){
      result += str[i] - '0';
      if (i!=str.size()-1){
         result = result * 10;
      }
   }
   return result;
}


bool is_num(string word){
   for (unsigned i=0; i<word.size(); i++){
      if (!((word[i]>='0')&&(word[i]<='9'))){
         return false;
      }
   }
   return true;
}


long convert_str_2_long(string str){
   long result = 0;
   for (unsigned i=0; i<str.size(); i++){
      if ((str[i]>='a')&&(str[i]<='f')){
         result += str[i] -'a'+10;
      }
      else if ((str[i]>='0')&&(str[i]<='9')){
         result += str[i] - '0';
      }
      if (i!=str.size()-1){
         result = result * 16;
      }
   }
   return result;
}


bool is_hex(string word){
   if (word.size()==0) return false;
   for (unsigned int i=0; i<word.size(); i++){
      if (!(((word[i]>='0')&&(word[i]<='9'))||((word[i]>='a')&&(word[i]<='f')))){
         return false;
      }
   }
   return true;
}


uint8_t convert_str_2_uint8(string str){
   uint8_t result = 0;
   if ((str[0]>='a')&&(str[0]<='f')){
      result += str[0] -'a'+10;
   }
   else if ((str[0]>='0')&&(str[0]<='9')){
      result += str[0] - '0';
   }
   result = result * 16;
   if ((str[1]>='a')&&(str[1]<='f')){
      result += str[1] -'a'+10;
   }
   else if ((str[1]>='0')&&(str[1]<='9')){
	   result += str[1] - '0';
   }
   return result;
}



vector<uint8_t> convert_long_2_vec_uint8(long input){
   vector<uint8_t> tmp;
   long divisor = 256*256*256;
   long input_ = input;
   long tmp1;
   for (int i=0 ; i<4; i++){
      tmp1 = input_/divisor;
      input_ = input_%divisor;
      tmp.push_back((uint8_t)tmp1);
      divisor /=256;
   }
   vector<uint8_t> result;
   for (int i=3 ; i>=0; i--){
      result.push_back(tmp[i]);
   }
   return result;
}



string convert_long_2_hex_string(long num){
   stringstream stream;
   stream << std::hex << num;
   string result( stream.str() );
   return result;
}


vector<long> get_keys_to_array(unordered_map<long, func_info> func){
   vector<long> addr_vec;
   for (auto it = func.begin(); it!=func.end(); it++){
      addr_vec.push_back(it->first);
   }
   return addr_vec;
}



vector<long> get_moved_addr_to_array(unordered_map<long, func_info> func){
   vector<long> addr_vec;
   for (auto it = func.begin(); it!=func.end(); it++){
      addr_vec.push_back(it->second.moved_addr);
   }
   return addr_vec;
}


char** split_str_2_char_array(const string &str){
   vector<string> word_vec = split_line(str);
   char** words;
   words = (char**)malloc(sizeof(char*)*word_vec.size()+1);
   for (unsigned i=0; i<word_vec.size(); i++){
      words[i] = (char*)malloc(sizeof(char)*word_vec[i].size()+1); 
      memset(words[i], '\0', sizeof(char)*(word_vec[i].size()+1));
      strcpy(words[i], word_vec[i].c_str());
   }
   return words;
}


void clean_up(const ocolos_env* ocolos_environ){
   string command = "rm -rf "+ocolos_environ->tmp_data_path+"bolted_functions.bin"+
                    ocolos_environ->tmp_data_path+" call_sites.bin"+
                    ocolos_environ->tmp_data_path+" v_table.bin"+
                    ocolos_environ->tmp_data_path+" unmoved_func.bin"+
                    ocolos_environ->tmp_data_path+" *.data"+
                    ocolos_environ->tmp_data_path+" *.fdata"+
                    ocolos_environ->tmp_data_path+" *.txt"+
                    ocolos_environ->tmp_data_path+" *.old";
   if (system(command.c_str())==-1) exit(-1);
}
