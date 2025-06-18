#include <vector>
#include <iostream>
#include <stdio.h>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <thread>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "boost_serialization.hpp"
#include "utils.hpp"
#include <libelf.h>
#include <gelf.h>
#include <capstone/capstone.h>
#include <sys/stat.h>
#include <sys/mman.h>
#define N 16

using namespace std;

void thread_function(vector<pair<long, long>> func_name, unordered_map<long, call_site_info> &call_sites, string new_target_binary, ocolos_env* ocolos_environ){
   for (unsigned i=0; i<func_name.size(); i++){
      FILE *fp;
      char path[1000];
      string start_addr = convert_long_2_hex_string(func_name[i].first);
      string stop_addr = convert_long_2_hex_string(func_name[i].first + func_name[i].second);

      uint64_t start_addr_ = convert_str_2_long(start_addr);
      uint64_t stop_addr_ = convert_str_2_long(stop_addr);

      int fd = open(new_target_binary.c_str(), O_RDONLY);
      if (fd < 0) {
        perror("open");
        exit(0);
      }

      struct stat st;
      fstat(fd, &st);
      void *map = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
      if (map == MAP_FAILED) { perror("mmap"); close(fd); return; }

      Elf *e = elf_begin(fd, ELF_C_READ, NULL);
      if (!e) {
        fprintf(stderr, "elf_begin() failed.\n");
        close(fd);
        exit(0);
      }

      size_t shstrndx;
      elf_getshdrstrndx(e, &shstrndx);

      Elf_Scn *scn = NULL;
      GElf_Shdr shdr;
      while ((scn = elf_nextscn(e, scn)) != NULL) {
        gelf_getshdr(scn, &shdr);
        char *name = elf_strptr(e, shstrndx, shdr.sh_name);
        if (strcmp(name, ".text") == 0) {      

          uint64_t section_addr = shdr.sh_addr;
          uint64_t section_offset = shdr.sh_offset;

          if (start_addr_ < section_addr || stop_addr_ > section_addr + shdr.sh_size) {
            cout<<"@@@ "<<start_addr<<", "<<stop_addr<<"\n";
            fprintf(stderr, "start addr: 0x%lx, stop addr: 0x%lx\n", start_addr_, stop_addr_);
            fprintf(stderr, "Address range not in .text section\n");
            break;
          }

          size_t offset_start = section_offset + (start_addr_ - section_addr);
          size_t size = stop_addr_ - start_addr_;
          uint8_t *code = (uint8_t *)map + offset_start;

          // Capstone disassemble
          csh handle;
          cs_insn *insn;
          size_t count;

          if (cs_open(CS_ARCH_X86, CS_MODE_64, &handle) != CS_ERR_OK)
            break;

          count = cs_disasm(handle, code, size, start_addr_, 0, &insn);
          for (size_t j = 0; j < count; j++) {
            printf("0x%" PRIx64 ": %s %s\n", insn[j].address, insn[j].mnemonic, insn[j].op_str);
          }

          cs_free(insn, count);
          cs_close(&handle);
          break;
        }
      }
      
      elf_end(e);
      munmap(map, st.st_size);
      close(fd);      

      string command = ""+ocolos_environ->objdump_path+" -d --start-addr=0x" + start_addr + " --stop-addr=0x" + stop_addr + " " + new_target_binary;
      char * command_cstr = new char [command.length()+1];
      strcpy (command_cstr, command.c_str());

      fp = popen(command_cstr, "r");
      while (fp == NULL) {
         printf("Failed to run command\n" );
         printf("function addr: 0x%lx\n", func_name[i].first);
         sleep(1);
         fp = popen(command_cstr, "r");
         //exit(-1);
      }

      vector<string> assembly_lines;
      while (fgets(path, sizeof(path), fp) != NULL) {
         string line(path);
         assembly_lines.push_back(line);
      }
      fclose(fp);


      for (unsigned k=0; k<assembly_lines.size()-1; k++){
         string line = assembly_lines[k];
         vector<string> words = split_line(line);
         if ((words.size()>8) && ((words[6]=="call")||(words[6]=="callq"))){
            // if it's a library call
            if (words[8].substr(words[8].size()-5, words[8].size()-1)=="@plt>") continue;
            call_site_info call_info;
            //cout<<line;
            call_info.addr = convert_str_2_long(words[0].substr(0,words[0].size()-1));
            call_info.belonged_func = func_name[i].first;

            string next_line = assembly_lines[k+1];
            vector<string> words_next_line = split_line(next_line);

            // if there is no instruction after the call instruction
            if (words_next_line.size()==0) continue;

            call_info.next_addr = convert_str_2_long(words_next_line[0].substr(0,words_next_line[0].size()-1));
            int idx = 0;
            vector<string> binary_strs;

            for (unsigned j=0; j<words.size(); j++){
               if ((words[j].length()==2)&&(is_hex(words[j]))){
                  if (idx==5) {
                     printf("[extract_call_site]: error in the size of a call instruction\n");
                     exit(-1);
                  }
                  binary_strs.push_back(words[j]);
                  call_info.machine_code[idx] = convert_str_2_uint8(words[j]);
                  idx++;
               }
            }

            string real_offset = "";
            if (binary_strs.size()!=5) {
               cout<<"[extract_call_site]: error: binary_strs.size()!=5\n";
            }
            for (unsigned j=binary_strs.size()-1; j>0; j-- ){
               string tmp = real_offset+binary_strs[j];
               real_offset = tmp;
            }
            call_info.target_func_addr = 0xffffffff & (convert_str_2_long(real_offset) + call_info.next_addr);
            call_sites[call_info.addr]=call_info;
         }
      }
   }
}



int main(){
   FILE *fp3;
   char path3[1000];
   ocolos_env ocolos_environ;
   if (elf_version(EV_CURRENT) == EV_NONE) {
     fprintf(stderr, "ELF library initialization failed.\n");
     return 1;
   }
 
   string command = "cp "+ocolos_environ.target_binary_path+" "+ocolos_environ.tmp_data_path;
   system(command.c_str());

   vector<string> dir_names = split(ocolos_environ.target_binary_path, '/');
   string new_target_binary = ocolos_environ.tmp_data_path + dir_names[dir_names.size()-1];

   command = "strip -g "+new_target_binary;
   system(command.c_str());

   command = ""+ocolos_environ.nm_path+" -S -n "+new_target_binary;	
   char* command_cstr = new char[command.length()+1];
   strcpy (command_cstr, command.c_str());
   fp3 = popen(command_cstr, "r");
   if (fp3 == NULL){
      printf("Failed to run nm on BOLTed binary\n" );
      exit(-1);
   }

   vector<pair<long, long>> func_name;
   while (fgets(path3, sizeof(path3), fp3) != NULL) {
      string line(path3);
      vector<string> words = split_line(line);
      if (words.size()>3){
         cout<<line<<endl;
         if ((words[2]=="T")||(words[2]=="t")||(words[2]=="W")||(words[2]=="w")){
            long start_addr = convert_str_2_long(words[0]);
            long len_str = convert_str_2_long(words[1]);
            func_name.push_back(make_pair(start_addr, len_str));
         }
      }  
   }

   printf("[extract_call_sites] %ld functions in the original binary \n", func_name.size());
   fclose(fp3);

   int size =(int)( func_name.size()/N);
	
   vector<vector<pair<long, long>>> func_names;
   for (int i=0; i<N; i++){
      if (i!=(N-1)){
         vector<pair<long, long>>::const_iterator first = func_name.begin()+i*size;
         vector<pair<long, long>>::const_iterator last = func_name.begin()+(i+1)*size;
         vector<pair<long, long>> new_str(first, last);
         func_names.push_back(new_str);
      }
      else{
         vector<pair<long, long>>::const_iterator first = func_name.begin()+i*size;
         vector<pair<long, long>>::const_iterator last = func_name.end();
         vector<pair<long, long>> new_str(first, last);
         func_names.push_back(new_str);
      }
   }
   unordered_map<long, call_site_info> call_sites_array[N];
	
   vector<thread> threads;	
   for (int i=0; i<N; i++){
      thread t(thread_function, func_names[i], ref(call_sites_array[i]), new_target_binary, &ocolos_environ);		
      threads.push_back(std::move(t));
   }	

   for (int i=0; i<N; i++){
      threads[i].join();
      printf("the size of call_sites = %lu\n", call_sites_array[i].size());
   }

   unordered_map<long, call_site_info> call_sites;
   for (int i=0; i<N; i++){
      call_sites.insert(call_sites_array[i].begin(), call_sites_array[i].end());
   }
   printf("@@@@@@@@@@@@ the size of call_sites (final) = %lu\n", call_sites.size());

   ofstream ofs(ocolos_environ.call_sites_all_bin);
   boost::archive::binary_oarchive oa(ofs);
   oa << call_sites;

   unordered_map<long, vector<long> > call_sites_list;
   for (auto it = call_sites.begin(); it != call_sites.end(); it++){
      if (call_sites_list.find(it->second.target_func_addr)!=call_sites_list.end()){
         call_sites_list[it->second.target_func_addr].push_back(it->first);
      }	
      else{
         vector<long> tmp;
         tmp.push_back(it->first);
         call_sites_list.insert(make_pair(it->second.target_func_addr, tmp));
      }
   }
   printf("########### the size of call_sites_list = %lu\n", call_sites_list.size());

   ofstream ofs1(ocolos_environ.call_sites_list_bin);
   boost::archive::binary_oarchive oa1(ofs1,boost::archive::no_codecvt);
   oa1 << call_sites_list;

//   command = "rm -rf "+new_target_binary;
   system(command.c_str());
}
