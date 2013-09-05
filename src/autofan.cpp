#include "autofan.h"

const boost::regex autofan::rules[] = 
{ boost::regex(".*add.*me", boost::regex::icase),
  boost::regex(".*sign.*me", boost::regex::icase),
  boost::regex(".*count.*me.*in", boost::regex::icase),
  boost::regex(".*I.*join", boost::regex::icase)
};

const unsigned int autofan::n_rules = 4;

//We don't want to create the object like this, currently unsupported
autofan::autofan(void)
{
    id = "";    
}

autofan::autofan(std::string topic_id)
{
   id = topic_id; 
}

void autofan::get_fans(void)
{
    tltopic* fanclub = new tltopic(id);
    fanclub->fetch_pages();
    tltopic::post* posts = fanclub->get_posts();
    unsigned int post_count = fanclub->get_post_count();
    unsigned int i;
    unsigned int j;
    bool         is_fan;
    for (i = 0; i < post_count; i++)
    {
        is_fan = false;
        for (j = 0; j < n_rules; j++)
        {
           
           if (regex_search(posts[i].content, rules[j])) 
                is_fan = true;
        }     
        if (is_fan)
            std::cout << posts[i].u_name << std::endl;
        std::cerr << posts[i].u_name << std::endl;
        std::cerr << posts[i].content << std::endl;
    }
    delete [] posts;
    delete fanclub;
}
