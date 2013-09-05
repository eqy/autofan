#ifndef TLTOPIC_H
#define TLTOPIC_H
#include <string>
#include <curl/curl.h>
#include <tidy/tidy.h>
#include <tidy/buffio.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/HTMLparser.h>
#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <cstdlib> 

   

class tltopic
{
    public: 
    //There will be more options later
    enum parse_status_t
    {
        GOOD,
        BAD
    };

    typedef struct post
    {
        std::string u_name;
        std::string content;
        post* next;
    }post;   
    
    static const std::string PREFIX;
    static const std::string SUFFIX;
    static const boost::regex JUICY_KEY;
    static const boost::regex VALID_POST_HEADER;
    static const char * HTML;
    static const char * BODY;
    static const char * TABLE;
    static const char * TABLE_DATA;
    static const char * TEXT;
    static const char   TAB;
    static const char   NEWLINE;

                    tltopic(void);
                    tltopic(std::string topic_id);
                    ~tltopic(void);
    void            set_id(std::string topic_id);
    std::string     get_id(void);
    std::string     get_url(void);
    void            fetch_pages(void);
    void            download_page(std::string page_url);
    //Static because userdata pointer is given to our callback so it doesn't
    //need to know which object it is associated with--non-static implementation
    //may unnecessairly complicate libcurl interaction
    static size_t   download_data(const char *buf,const size_t size,const size_t\
        nmemb,const  void *userdata);
    //For debugging purposes only, why on earth would we want raw html shoved in
    //a terminal
    std::string     get_current_page(void);
    std::string     get_current_tidied(void);
    parse_status_t  parse_page(void); 
    post*           get_posts(void);
    unsigned int    get_post_count(void);    
 
    private:
    static xmlNode*    find_juicy_table(const xmlNode* root_node); 
    static bool        has_juicy_key(std::string input);
    void               extract_posts(const xmlNode* post_table);
    post*              parse_post(const xmlNode* post); 
    static bool        is_valid_post(const std::string post_header);
    static std::string scrape_u_name(const std::string post_header);
    static std::string cleanup(const std::string input);
    void               free_posts(void);
         
    std::string     id; 
    std::string     url;
    std::string     current_page;
    std::string     current_tidied;
    CURL*           easy_handle; 
    TidyDoc         tidy_instance;
    post*           head;
    post*           tail;
    unsigned int    post_count;
};
#endif
