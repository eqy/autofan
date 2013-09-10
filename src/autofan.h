#ifndef AUTOFAN_H
#define AUTOFAN_H
#include "tltopic.h"
#include <iostream>
#include <map>

class autofan
{
    public:
    static const boost::regex rules[]; 
    static const unsigned int n_rules;
    autofan(void);
    autofan(std::string topic_id);
    void get_fans(void);
   

    private:
    std::string id;
};

#endif
