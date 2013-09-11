#ifndef AUTOFAN_H
#define AUTOFAN_H
#include "tltopic.h"
#include <iostream>
#include <map>
#include <time.h>

class autofan
{
    public:
    static const boost::regex rules[]; 
    static const unsigned int n_rules;
    static const char *       postgres_params;
    autofan(void);
    autofan(std::string topic_id);
    void get_fans(void);
    void update_db(void);
   

    private:
    std::string id;
    std::string fan_list;
};

#endif
