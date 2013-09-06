#include "autofan.h"

const boost::regex autofan::rules[] = 
{ boost::regex("\\s?add\\s?me", boost::regex::perl | boost::regex::icase),
  boost::regex("\\s?sign\\s?me", boost::regex::perl | boost::regex::icase),
  boost::regex("\\s?count\\s?me\\s?in", boost::regex::perl | boost::regex::icase),
  boost::regex("\\s?I'll\\s?join", boost::regex::perl | boost::regex::icase),
  boost::regex("\\s?go?", boost::regex::perl | boost::regex::icase),
  boost::regex("\\s?me\\s?up", boost::regex::perl | boost::regex::icase)
};

const unsigned int autofan::n_rules = 6;

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
           
           if (regex_search(posts[i].content, rules[j]) || i==0) 
                is_fan = true;
        }     
        if (is_fan)
            std::cout << posts[i].u_name << std::endl;
        std::cerr << posts[i].u_name << std::endl;
        std::cerr << posts[i].content << std::endl;
    }
    if (posts != NULL)
    {
        delete [] posts;
    }
    delete fanclub;
}
