#include <boost/regex.hpp>
#include <iostream>
#include "tltopic.h"
#include "autofan.h"

int main(int, char* argv[])
{
   if(argv[1] == NULL)
   return 0;
 
   autofan* myautofan = new autofan(std::string(argv[1]));
   //std::cout << fanclub->get_id() << std::endl;
   //std::cout << fanclub->get_url() << std::endl;
   myautofan->get_fans();
   myautofan->update_db();
   //std::cout << fanclub->get_index_page() << std::endl;
   //fanclub->parse_index();
   //std::cout << fanclub->get_index_tidied() << std::endl;
   delete myautofan;
}
