#include <string>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/iostreams/stream.hpp>

using namespace std;

typedef class call_site_information{
   private:
      friend class boost::serialization::access;
      /* When the class Archive corresponds to an output archive, the
       * & operator is defined similar to <<.  Likewise, when the class 
       * Archive
       * is a type of input archive the & operator is defined similar 
       * to >>.
       */
      template<class Archive>
      void serialize(Archive & ar, const unsigned int version){
         ar & machine_code;
         ar & addr;
         ar & next_addr;
         ar & belonged_func;
         ar & target_func_addr;
      }
      public:
         uint8_t machine_code[5];	// the machine code of the call insn

         /*
          * The address of the call instruction
          */
         long addr;



         /*
          * The address of the next instruction after the call site
          */
         long next_addr;



         /*
          * The starting address of the function that the call site
          * belongs to.
          */
         long belonged_func;



         /*
          * In the call instruction, there is a function pointer 
          * that points to the callee function. This variable stores
          * the value of the function pointer to the callee function. 
          */
         long target_func_addr;



         /*
          * constructor function.
          */
         call_site_information(){};
} call_site_info;
