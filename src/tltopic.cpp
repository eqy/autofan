#include "tltopic.h"
#define SOFT_PAGE_SIZE 524288

const std::string tltopic::PREFIX = "http://www.teamliquid.net/forum/viewmessage.php?topic_id=";
const std::string tltopic::SUFFIX = "&currentpage=";
const boost::regex tltopic::JUICY_KEY("Gift.*TL+.*Please.*log.*in.*.or.*register.*to.*.*reply.");
const boost::regex tltopic::VALID_POST_HEADER(".*Posts.*");
const char * tltopic::HTML        = "html";
const char * tltopic::BODY        = "body";
const char * tltopic::TABLE       = "table";
//const char * tltopic::TABLE_DATA  = "TD";
const char * tltopic::TEXT   = "text";
const char tltopic::TAB    = '\t';
const char tltopic::NEWLINE = '\n';

tltopic::tltopic(void)
{
    id = "0";
    url = PREFIX;
    url.append(id);
    easy_handle = curl_easy_init();      
    head = NULL;
    tail = NULL;
    post_count = 0;
}

tltopic::tltopic(std::string topic_id)
{
    id = topic_id;
    url = PREFIX;
    url.append(id);
    easy_handle = curl_easy_init();
    head = NULL;
    tail = NULL;
    post_count = 0;
}

tltopic::~tltopic(void)
{
    curl_easy_cleanup(easy_handle);
    //We need to delete the linked list of posts
    free_posts();
}

void tltopic::set_id(std::string topic_id)
{
    id = topic_id;
}

std::string tltopic::get_id(void)
{
    return id;
}

std::string tltopic::get_url(void)
{
    return url;
} 

void tltopic::fetch_pages(void)
{
    //We are refreshing the page, so clear it and reserve a good chunk of memory
    //so we aren't constantly reallocating when the page is being downloaded
    //in chunks.
    //TODO: Add a check for allocation
    current_page = "";
    current_page.reserve(SOFT_PAGE_SIZE);
    download_page(url);
    
    parse_page();   
    int i = 2;
    std::stringstream page_url;
    while (true)
    {
        current_page = "";
        page_url.str("");
        page_url << url.c_str() << SUFFIX << i;
        download_page(page_url.str());
        parse_status_t parse_status = parse_page();
        if (parse_status == BAD)
        break;
        else
        std::cerr<<page_url.str()<<std::endl;
        i++; 
    }
}

void tltopic::download_page(std::string page_url)
{
    //Set parameters for page download: (1) the url, (2) the pointer to our
    //callback function which handles the downloaed data, (3) the pointer to our
    //data variable which the callback function uses
    curl_easy_setopt(easy_handle, CURLOPT_URL, page_url.c_str());        
    curl_easy_setopt(easy_handle, CURLOPT_WRITEFUNCTION,\
        tltopic::download_data);
    curl_easy_setopt(easy_handle, CURLOPT_WRITEDATA, &current_page); 
    curl_easy_perform(easy_handle);
}

size_t tltopic::download_data(const char *buf, const size_t size, const size_t nmemb, const void *userdata)
{
   //Append the buffer contents to the current page
   std::string * page = (std::string *) userdata;
   page->append(buf, nmemb);
   return size*nmemb;
}

std::string tltopic::get_current_page(void)
{
    return current_page;
}

std::string tltopic::get_current_tidied(void)
{
    return current_tidied;
}

tltopic::post* tltopic::get_posts(void)
{
    tltopic::post* posts_copy = new post[post_count];
    tltopic::post* curr       = head;
    unsigned int i = 0;
    posts_copy[i].u_name = std::string(curr->u_name);
    posts_copy[i].content = std::string(curr->content);
    curr = curr->next;
    while (curr != NULL)
    {
        i++;
        posts_copy[i].u_name = std::string(curr->u_name);
        posts_copy[i].content = std::string(curr->content);
        posts_copy[i-1].next = &posts_copy[i];
        posts_copy[i].next   = NULL;
        curr = curr->next;
    }
    return posts_copy;
}

unsigned int tltopic::get_post_count(void)
{
    return post_count;
}

tltopic::parse_status_t tltopic::parse_page(void)
{
    TidyBuffer output;
    //Slightly uglier than = {0}, but gets g++ to stop barking warnings at us
    memset(&output, 0, sizeof(output));
    TidyBuffer errbuf;
    memset(&errbuf, 0, sizeof(errbuf));
    bool ok;
    int  status = -1;
    char * output_xml = NULL;
    TidyDoc tidy_instance = tidyCreate();
    //Protip: The options are not listed in the API documentation--so just read
    //the source!
    ok = tidyOptSetBool(tidy_instance, TidyXmlOut, yes);
    if (ok)
        //Needed to fix &nbsp entries preserved 
        ok = tidyOptSetBool(tidy_instance, TidyNumEntities, yes);
    if (ok)
        ok = tidyOptSetBool(tidy_instance, TidyHideComments, yes);
    if (ok)
        status = tidySetErrorBuffer(tidy_instance, &errbuf);
    if (status >= 0)
        status = tidyParseString(tidy_instance, current_page.c_str()); 
    if (status >= 0)
        status = tidyCleanAndRepair(tidy_instance);
    if (status > 1)
        status = tidyOptSetBool(tidy_instance, TidyForceOutput, yes);
    if (status >= 0)
        status = tidySaveBuffer(tidy_instance, &output);
            
    //Show all DEBUG information from libtidy
    output_xml = (char *) output.bp;
    std::cerr << errbuf.bp << std::endl;
    current_tidied = output_xml; 
   
    xmlDocPtr current_xml_doc = NULL;
    //Let's try to make it work even if the page isn't very clean
    current_xml_doc = htmlReadMemory((const char *)output.bp,\
strlen(output_xml),  url.c_str(), NULL, HTML_PARSE_RECOVER);    
    if(current_xml_doc != NULL)    
    {
        xmlNodePtr root_ptr  = xmlDocGetRootElement(current_xml_doc);
        xmlNodePtr table_ptr = find_juicy_table(root_ptr);
        //means that the current page was bad and there is no valid output
        if (table_ptr == root_ptr)
        {
            tidyRelease(tidy_instance);
            tidyBufFree(&output);
            tidyBufFree(&errbuf);
            xmlFreeDoc(current_xml_doc);
            return BAD;
        }
        else 
        {
            extract_posts(table_ptr);
        }
    }
    else
    {
        tidyRelease(tidy_instance);
        tidyBufFree(&output);
        tidyBufFree(&errbuf);
        xmlFreeDoc(current_xml_doc);
        return BAD;
    }
    tidyRelease(tidy_instance);
    tidyBufFree(&output);
    tidyBufFree(&errbuf);
    xmlFreeDoc(current_xml_doc);
    return GOOD;
}

xmlNode* tltopic::find_juicy_table(const xmlNode* root_nodes)
{  
   xmlNode* curr = (xmlNode *) root_nodes;
   if (curr == NULL)
   return curr;
   
   xmlNode* curr_child = curr;
   while (curr_child != NULL)
   {
       char * temp_content = (char *) xmlNodeGetContent(curr_child);
       if(has_juicy_key(std::string((char *) temp_content)))
       {
            std::cerr << "found juicy key in a child, moving down a layer" <<\
            std::endl;
            curr = curr_child;
            curr_child = curr_child->children;
       } 
       else
       {
            std::cerr << "didn't find the juicy key in a child yet, going to the next child" << std::endl;
            curr_child = curr_child->next;
       }
       xmlFree(temp_content);
   }
   return curr;
}

bool tltopic::has_juicy_key(std::string input)
{
    return regex_search(input, JUICY_KEY);     
}

//This method is likely the least robust to post-layout changes
void tltopic::extract_posts(const xmlNode* post_table)
{
    //We get the table of posts, so its children are the individual posts
    xmlNode* curr = (xmlNode*) post_table->children;
    xmlNode* curr_children;
    while(curr != NULL)
    {
       //The posts are actually buried in the table rows
       if(strcmp((char *) curr->name, "tr")==0)
        {
            curr_children = curr->children;
            while (curr_children != NULL)
            {
                //We are only interested in the data elements of the rows
                //Each data element corresponds to an individual posts
                if (strcmp((char *) curr_children->name, "td")==0)
                {
                    //Manage addition of newly parsed posts to the linked list
                    post* new_post = parse_post(curr_children);
                    //Only add the new post if a valid pointer is returned--a
                    //NULL pointer signifies that a valid post could not be
                    //generated from the current table entry
                    if (new_post != NULL)
                    {
                        post_count++;
                        if (head == NULL)
                        {
                            head = new_post;
                            tail = new_post;
                        }
                        else
                        {
                            tail->next = new_post;
                            tail      = new_post;
                        }
                    }
                }
                curr_children = curr_children->next;
            }
        }
        curr = curr->next;
    }
}

tltopic::post* tltopic::parse_post(const xmlNode* post_node)
{
    tltopic::post* new_post = new post();
    xmlNode* curr_children = post_node->children;
    while (curr_children != NULL)
    {
        if (strcmp((char *)curr_children->name,TABLE) == 0)
        {
            xmlNode* temp = curr_children->children;
            //subtext = 0 means we are reading the header of the post
            //subtext = 1 means we are reading the content of the the post
            //subtext = 2 means we are reading the signature of the post (if it
            //exists)
            int subtext = 0;
            while (temp != NULL)
            {
                if (strcmp((char *)temp->name, TEXT))
                {
                    //See what we are dealing with
                    char * temp_content = (char *) xmlNodeGetContent((xmlNode*) temp);
                    //Move on if the header of the post is invalid and return a
                    //NULL pointer
                    if (subtext == 0 && !is_valid_post(std::string(temp_content)))
                    {
                        xmlFree(temp_content);
                        delete new_post;
                        new_post = NULL;
                        break;
                    }
                    //We don't process the signature
                    else if (subtext == 2)
                    {
                        xmlFree(temp_content);
                        break;
                    }
                    //This corresponds to the header; We scrape the header.
                    if (subtext == 0)
                    {
                        new_post->u_name = scrape_u_name(std::string(temp_content));
                        //std::cerr << scrape_u_name(std::string(temp_content)) << std::endl;
                    }
                    //This corresponds to the content; We scrape the content.
                    else
                    {
                        new_post->content = cleanup(std::string(temp_content));
                        //std::cerr << cleanup(std::string(temp_content)) << std::endl;
                    }
                    xmlFree(temp_content);
                    subtext++;
                }
                
                temp = temp->next; 
            } 
                std::cerr << std::endl;
        }
                curr_children = curr_children->next;
    }
    return new_post;   
}

bool tltopic::is_valid_post(const std::string post_header)
{
    return regex_search(post_header, VALID_POST_HEADER);
}

std::string tltopic::scrape_u_name(const std::string post_header)
{
   std::string cleaned_header = cleanup(post_header);
   unsigned int header_len = cleaned_header.length();
   unsigned int i = 0;
   std::string u_name = "";
   bool reached_text = false;
   while (i < header_len)
   {
        char current = cleaned_header.at(i);
        if (!reached_text)
        {

            if (!isspace(current) && current!=TAB && current !=NEWLINE)
            {
                u_name.append(1, current);
                reached_text = true;
            
            }
        }
        else
        {
            if (!isspace(current) && current!=TAB && current !=NEWLINE)
            {
                u_name.append(1, current);
            }
            else
                break;    
        }    
       i++; 
   }     
   return u_name;
}

//We remove any nasty preceding or trailing whitespace and especially disgusting
//html specific characters that made it through the parser.
std::string tltopic::cleanup(const std::string input)
{
   std::string processed = "";
   std::string trimmed_input(input);
   boost::trim(trimmed_input);
   unsigned int input_len = trimmed_input.length();
   unsigned int i = 0;
   char     current = 0;
   while (i < input_len)
   {
        current = trimmed_input.at(i);
        if (isprint(current) || current == NEWLINE)
            processed.append(1, current);
        i++;
   } 
   return processed;
}

void tltopic::free_posts(void)
{
    post* curr = head;
    post* temp = NULL;
    while(curr != NULL)
    {
        temp = curr->next;
        delete curr;
        curr = temp;
    } 
}
