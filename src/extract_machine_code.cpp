#include "extract_machine_code.hpp"
#include "boost_serialization.hpp"

using namespace std;

void extract_call_sites(FILE* pFile, unordered_map<long, func_info> moved_func, unordered_map<long, func_info> func_in_call_stack, const ocolos_env* ocolos_environ){
   // <starting address, call_sites_info>
   unordered_map<long, call_site_info> call_sites;
   const string file = ocolos_environ->call_sites_all_bin;
   ifstream filestream(file);
   boost::archive::binary_iarchive archive(filestream);
   archive >> call_sites;

   // <target address, caller inst addresses>
   unordered_map<long, vector<long> > call_sites_list;
   ifstream filestream1(ocolos_environ->call_sites_list_bin);
   boost::archive::binary_iarchive archive1(filestream1);
   archive1 >> call_sites_list;

   for (auto it = moved_func.begin(); it!=moved_func.end(); it++){
      if (call_sites_list.find(it->first)!=call_sites_list.end()){
         vector<long> caller_lists = call_sites_list[it->first];
         for (unsigned i=0; i<caller_lists.size(); i++){
            long addr = caller_lists[i];
            long belonged_func = call_sites[addr].belonged_func;
            if (func_in_call_stack.find(belonged_func)==func_in_call_stack.end()) continue;

            long base_addr = call_sites[addr].next_addr;
            long new_target_addr = it->second.moved_addr;
            long offset = new_target_addr - base_addr;
            vector<uint8_t> machine_code_line = convert_long_2_vec_uint8(offset);
            // write the virtual address + the size of the machine code
            // and machine code itself into a file
            long machine_code_address = addr;
            long machine_code_size = (long)(machine_code_line.size()+1);
            fwrite(&machine_code_address, sizeof(long), 1, pFile);
            fwrite(&machine_code_size, sizeof(long), 1, pFile);
            uint8_t buffer[machine_code_line.size()+1];
            
            buffer[0]= (uint8_t)232;
            for (unsigned i=0; i<machine_code_line.size(); i++){
               buffer[i+1] = machine_code_line[i];
            }
            fwrite (buffer , sizeof(uint8_t), machine_code_line.size()+1, pFile);
         }  
      }
   }
}



