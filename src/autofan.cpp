#include "autofan.h"
#include <libpq-fe.h>

const boost::regex autofan::rules[] = 
{ boost::regex("\\s?add\\s?me", boost::regex::perl | boost::regex::icase),
  boost::regex("\\s?sign\\s?me", boost::regex::perl | boost::regex::icase),
  boost::regex("\\s?count\\s?me\\s?in", boost::regex::perl | boost::regex::icase),
  boost::regex("\\s?I'll\\s?join", boost::regex::perl | boost::regex::icase),
  boost::regex("\\s?go?", boost::regex::perl | boost::regex::icase),
  boost::regex("\\s?me\\s?up", boost::regex::perl | boost::regex::icase)
};

const unsigned int autofan::n_rules = 6;
const char *       autofan::postgres_params = "dbname = fandb";

//We don't want to create the object like this, currently unsupported
autofan::autofan(void)
{
    id = "";    
    fan_list = "";
}

autofan::autofan(std::string topic_id)
{
   id = topic_id; 
   fan_list = "";
}

void autofan::get_fans(void)
{
    tltopic* fanclub = new tltopic(id);
    fanclub->set_kill_quotes(true);
    fanclub->fetch_pages();
    
    unsigned int post_count = fanclub->get_post_count();
    if (post_count > 0)
    {
        std::map<std::string, unsigned int> * fan_map = new std::map<std::string, unsigned int>();
        tltopic::post* posts = fanclub->get_posts();
        unsigned int i;
        unsigned int j;
        bool         is_fan;
        for (i = 0; i < post_count; i++)
        {
            is_fan = false;
            for (j = 0; j < n_rules; j++)
            {
               
               if (regex_search(posts[i].content, rules[j]) || i==0) 
               {
                    is_fan = true;
               } 
               
            }     
            if (is_fan && fan_map->find(posts[i].u_name) == fan_map->end())
            {
                std::cout << posts[i].u_name << std::endl;
                fan_list.append(posts[i].u_name);
                fan_list.append(1, '\n');
                (*fan_map)[posts[i].u_name] = 0;                    
            }
            std::cerr << posts[i].u_name << std::endl;
            std::cerr << posts[i].content << std::endl;
            std::cerr << "================================================================================"<<std::endl;
        }
        if (posts != NULL)
        {
            delete [] posts;
        }
        delete fan_map;
    }
    delete fanclub;
}

void autofan::update_db(void)
{
    PGconn *conn;
    PGresult *res;
    conn = PQconnectdb(postgres_params);
    if (PQstatus(conn) != CONNECTION_OK)
    {
        std::cerr << "postgres db connection failed" << std::endl;
    }
    else
    {
        
        bool found_table = false;
        res = PQexec(conn, "SELECT * FROM pg_catalog.pg_tables;");
        unsigned int n_tuples = 0;
        if (PQresultStatus(res) == PGRES_TUPLES_OK)
        {
            n_tuples = PQntuples(res);
            std::cerr << "We're in business" << std::endl;
            std::cerr << n_tuples << " tables" << std::endl;
            unsigned int i;
            for (i = 0; i < n_tuples; i++)
            {
                if (!strcmp(PQgetvalue(res,i,1), "topics"))
                {
                    std::cerr << "found table" << std::endl;
                    found_table = true;
                    break;
                }
            }
        }
        PQclear(res);
        if (!found_table)
        {
            //std::cerr << "Not quite" <<std::endl;
            //PQclear(res);
            //[res = PQexec(conn, "CREATE SCHEMA public");
            //PQclear(res);
            //std::cerr << PQntuples(res) << " tables found" << std::endl;
            std::cerr << "Attempting to create table" << std::endl;
            
            res = PQexec(conn, "CREATE TABLE IF NOT EXISTS topics(\
                         id char(10),\
                         fans text,\
                         date integer\
                             )"); 
            PQclear(res);
        }
            unsigned int curr_time = (unsigned int) time(NULL);
            std::cerr << curr_time << std::endl;
            std::cerr << fan_list << std::endl;
    }
    PQfinish(conn);
    return;
}
