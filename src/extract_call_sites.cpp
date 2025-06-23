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
#include <bfd.h>
#define N 8

using namespace std;

void thread_function(vector<pair<long, long>> func_name, unordered_map<long, call_site_info> &call_sites, string new_target_binary, ocolos_env* ocolos_environ){
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

   for (unsigned i=0; i<func_name.size(); i++){
      FILE *fp;
      char path[1000];
      string start_addr = convert_long_2_hex_string(func_name[i].first);
      string stop_addr = convert_long_2_hex_string(func_name[i].first + func_name[i].second);

      uint64_t start_addr_ = convert_str_2_long(start_addr);
      uint64_t stop_addr_ = convert_str_2_long(stop_addr);

      Elf_Scn *scn = NULL;
      GElf_Shdr shdr;
      while ((scn = elf_nextscn(e, scn)) != NULL) {
         gelf_getshdr(scn, &shdr);
         char *name = elf_strptr(e, shstrndx, shdr.sh_name);
         if (strcmp(name, ".text") == 0) {      

            uint64_t section_addr = shdr.sh_addr;
            uint64_t section_offset = shdr.sh_offset;

            if (start_addr_ < section_addr || stop_addr_ > section_addr + shdr.sh_size) {
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
            cs_detail *detail;
            size_t count;

            if (cs_open(CS_ARCH_X86, CS_MODE_64, &handle) != CS_ERR_OK)
               break;
            cs_option(handle, CS_OPT_DETAIL, CS_OPT_ON);

            count = cs_disasm(handle, code, size, start_addr_, 0, &insn);
            for (size_t j = 0; j < count; j++) {
               //printf("@@@ 0x%" PRIx64 ": %s %s\n", insn[j].address, insn[j].mnemonic, insn[j].op_str);
               if (insn[j].id == X86_INS_CALL && insn[j].detail != NULL) {
               printf("0x%" PRIx64 ": %s %s\n",
                      insn[j].address,
                      insn[j].mnemonic,
                      insn[j].op_str);
 
               detail = insn[j].detail;
               cs_x86 *x86 = &detail->x86;

               // We only care about direct call instructions
               if (x86->op_count > 0 && x86->operands[0].type == X86_OP_IMM) {
                  call_site_info call_info;
                  call_info.addr = insn[j].address;
                  call_info.belonged_func = func_name[i].first;

                  call_info.next_addr = insn[j+1].address;
                  uint64_t target = x86->operands[0].imm;
                  call_info.target_func_addr = target;

                  size_t inst_size = insn[j].size;
                  uint8_t *bytes = insn[j].bytes;
                  if (inst_size != 5) {
                     fprintf(stderr, "The size of call instruction is not 5 \n");
                  }
                  // Allocate array and copy bytes
                  memcpy(call_info.machine_code, bytes, inst_size);
                  call_sites[call_info.addr]=call_info;
               }
             }
           }

           cs_free(insn, count);
           cs_close(&handle);
           break;
         }
      }
   }
   elf_end(e);
   munmap(map, st.st_size);
   close(fd);
}



vector<pair<long, long>> extract_function_ranges(string filename) {
    bfd_init();

    bfd* abfd = bfd_openr(filename.c_str(), NULL);
    if (!abfd) {
        fprintf(stderr, "Failed to open binary: %s\n", filename.c_str());
        exit(1);
    }

    if (!bfd_check_format(abfd, bfd_object)) {
        fprintf(stderr, "Invalid format: %s\n", filename.c_str());
        bfd_close(abfd);
        exit(1);
    }

    if (!(abfd->flags & HAS_SYMS)) {
        fprintf(stderr, "No symbols found in file.\n");
        bfd_close(abfd);
        exit(1);
    }

    long symcount;
    unsigned int size;
    asymbol **syms;

    symcount = bfd_read_minisymbols(abfd, FALSE, (void**)&syms, &size);
    if (symcount == 0) {
        symcount = bfd_read_minisymbols(abfd, TRUE, (void**)&syms, &size);
    }

    vector<pair<long, long>> functions;

    for (long i = 0; i < symcount; i++) {
        asymbol* sym = syms[i];

        if (sym->flags & BSF_FUNCTION) {
            long start = (long)bfd_asymbol_value(sym);
            long length = 0;

            if (i + 1 < symcount) {
                long next = (long)bfd_asymbol_value(syms[i + 1]);
                if (next > start) {
                    length = next - start;
                }
            }

            functions.emplace_back(start, length);
        }
    }

    free(syms);
    bfd_close(abfd);
    return functions;
}





int main(){
   FILE *fp3;
   char path3[1000];
   ocolos_env ocolos_environ;
   if (elf_version(EV_CURRENT) == EV_NONE) {
     fprintf(stderr, "ELF library initialization failed.\n");
     return 1;
   }

   // strip the debug info otherwise the binary is too large 
   string command = "cp "+ocolos_environ.target_binary_path+" "+ocolos_environ.tmp_data_path;
   system(command.c_str());

   vector<string> dir_names = split(ocolos_environ.target_binary_path, '/');
   string new_target_binary = ocolos_environ.tmp_data_path + dir_names[dir_names.size()-1];

   command = "strip -g "+new_target_binary;
   system(command.c_str());

   // get all function: starting address, length
   vector<pair<long, long>> func_name = extract_function_ranges(new_target_binary);

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
