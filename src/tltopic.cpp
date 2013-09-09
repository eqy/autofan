#include "tltopic.h"
#define SOFT_PAGE_SIZE 524288

const std::string tltopic::PREFIX = "http://www.teamliquid.net/forum/viewmessage.php?topic_id=";
const std::string tltopic::SUFFIX = "&currentpage=";
const boost::regex tltopic::VALID_POST_HEADER(".*Posts.*");
const char * tltopic::HTML        = "html";
const char * tltopic::BODY        = "body";
const char * tltopic::TABLE       = "table";
const char * tltopic::POST_BODY_XPATH = "//td[@class='forumPost']";
//We have a choice here between keeping the code simple, or keeping it pretty.
//This is because tidy does not like the use of span and div in the post header
//in the original page source, so it inserts an empty span with the same
//attribute (forummsginfo) as the offending span. This causes libxml2 to double
//count post headers and leads to a mess. We avoid this by using an uglier
//(table) XPath expression.
//Should using the complicating XPath be necessary at some point, the fix would
//be to discard any post headers that are empty as the tidy-generated spans are
//empty.
//const char * tltopic::POST_HEADER_XPATH = "//span[@class='forummsginfo']";
const char * tltopic::POST_HEADER_XPATH = "//td[@valign='top'and @class='titelbalk']";
const char * tltopic::QUOTE_XPATH = "//div[@class='quote']";
//const char * tltopic::TABLE_DATA  = "TD";
const char * tltopic::TEXT   = "text";
const char tltopic::TAB    = '\t';
const char tltopic::NEWLINE = '\n';

tltopic::tltopic(void)
{
    id = "0";
    url = PREFIX;
    url.append(id);
    kill_quotes = false;
    easy_handle = curl_easy_init();      
    head = NULL;
    tail = NULL;
    tidy_instance = NULL;
    xml_doc_instance = NULL;
    xml_xpathctx_instance = NULL;
    post_count = 0;
}

tltopic::tltopic(std::string topic_id)
{
    id = topic_id;
    url = PREFIX;
    url.append(id);
    kill_quotes = false;
    easy_handle = curl_easy_init();
    head = NULL;
    tail = NULL;
    tidy_instance = NULL;
    xml_doc_instance = NULL;
    xml_xpathctx_instance = NULL;
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
    
    parse_status_t parse_status = parse_page(); 
    if (parse_status == BAD)
    {
        std::cerr << "returned" << std::endl;
        //std::cerr << post_count << std::endl;
        //std::cerr << head       << std::endl;
        return;
    }
    
    int i = 2;
    std::stringstream page_url;
    while (true)
    {
        current_page = "";
        page_url.str("");
        page_url << url.c_str() << SUFFIX << i;
        download_page(page_url.str());
        parse_status = parse_page();
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
    if (curr != NULL)
    {
        posts_copy[i].u_name = std::string(curr->u_name);
        posts_copy[i].content = std::string(curr->content);
        curr = curr->next;
    }
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

void tltopic::set_kill_quotes(bool choice)
{
    kill_quotes = choice;
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
    tidy_instance = tidyCreate();
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
   
    xml_doc_instance = NULL;
    //Let's try to make it work even if the page isn't very clean
    xml_doc_instance = xmlParseMemory((const char *)output.bp,strlen(output_xml));    
    xml_xpathctx_instance = xmlXPathNewContext(xml_doc_instance);
    
    tidyRelease(tidy_instance);
    tidyBufFree(&output);
    tidyBufFree(&errbuf);

    if (xml_xpathctx_instance != NULL)
    {
        parse_status_t parse_post_status = parse_posts();
        if (parse_post_status == BAD)
        {
            std::cerr << "Post parsing for current page failed" << std::endl;
            xmlFreeDoc(xml_doc_instance);
            xmlXPathFreeContext(xml_xpathctx_instance);
            return BAD;
        }
        else
        {  
            xmlFreeDoc(xml_doc_instance);
            xmlXPathFreeContext(xml_xpathctx_instance);
            return GOOD;
        }
    }
    else
    {
            xmlFreeDoc(xml_doc_instance);
            xmlXPathFreeContext(xml_xpathctx_instance);
            return BAD;
    }

    /*    
    if(xml_doc_instance != NULL)    
    {
        xmlNodePtr root_ptr  = xmlDocGetRootElement(xml_doc_instance);
        xmlNodePtr table_ptr = find_juicy_table(root_ptr);
        //means that the current page was bad and there is no valid output
        if (table_ptr == root_ptr)
        {
            tidyRelease(tidy_instance);
            tidyBufFree(&output);
            tidyBufFree(&errbuf);
            xmlFreeDoc(xml_doc_instance);
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
        xmlFreeDoc(xml_doc_instance);
        return BAD;
    }
    tidyRelease(tidy_instance);
    tidyBufFree(&output);
    tidyBufFree(&errbuf);
    xmlFreeDoc(xml_doc_instance);
    return GOOD;*/
    //return BAD;
}

/*
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
}*/

tltopic::parse_status_t tltopic::parse_posts(void)
{
    //If we want to ignore quotes
    if (kill_quotes)
    {
       xmlXPathObjectPtr quote_object =xmlXPathEvalExpression((xmlChar *) QUOTE_XPATH, xml_xpathctx_instance); 
       int n_quotes = quote_object->nodesetval->nodeNr;
       int q;
       for (q = n_quotes-1; q >= 0; q--)
       {
           xmlNodeSetContent(quote_object->nodesetval->nodeTab[q], (xmlChar *) "");           
       }
       if (quote_object != NULL)
       {
            xmlXPathFreeObject(quote_object);
       }
       
    } 

    //Get the post headers
    xmlXPathObjectPtr header_object = xmlXPathEvalExpression((xmlChar *) POST_HEADER_XPATH, xml_xpathctx_instance);
    int n_headers = header_object->nodesetval->nodeNr; 

    xmlXPathObjectPtr body_object   = xmlXPathEvalExpression((xmlChar *) POST_BODY_XPATH, xml_xpathctx_instance);
    int n_bodies  = body_object->nodesetval->nodeNr;

    //We have reached a nonexistent page in the topic
    if (n_headers == 0 || n_bodies == 0)
    {
        if (header_object != NULL)
            xmlXPathFreeObject(header_object);
        if (body_object != NULL)
            xmlXPathFreeObject(body_object);
        return BAD;
    }
    else if (n_headers != n_bodies)
    {
        std::cerr << "FATAL: Header/Body counter mismatch" << std::endl;
        std::cerr << "Header count: " << n_headers << std::endl;
        std::cerr << "Body count: " << n_bodies << std::endl;
        
        if (header_object != NULL)
            xmlXPathFreeObject(header_object);
        if (body_object != NULL)
            xmlXPathFreeObject(body_object);

        return BAD;
    }   
    else 
    {
        int i;
        //Make sure that the posts and headers are ordered properly
        //The API says that the nodes are in no particular order, though
        //experience suggests that this step not strictly necessary
        xmlXPathNodeSetSort(header_object->nodesetval);
        xmlXPathNodeSetSort(body_object->nodesetval);
        for (i = 0; i < n_headers; i++)
        {
            char * temp_header_content = (char *) xmlNodeGetContent(header_object->nodesetval->nodeTab[i]);
            char * temp_body_content   = (char *) xmlNodeGetContent(body_object->nodesetval->nodeTab[i]);
            if (tail != NULL)
            {
                post* temp = tail;
                tail = new post;
                tail->u_name = scrape_u_name(std::string(temp_header_content));
                tail->content = cleanup(std::string(temp_body_content));
                tail->next = NULL;
                temp->next = tail;
            } 
            else
            { 
                head = new post;
                head->u_name =  scrape_u_name(std::string(temp_header_content));
                head->content = cleanup(std::string(temp_body_content));
                head->next = NULL;
                tail = head;
            }            
            post_count++;
            xmlFree(temp_header_content);
            xmlFree(temp_body_content);
        } 
        
        if (header_object != NULL)
            xmlXPathFreeObject(header_object);
        if (body_object != NULL)
            xmlXPathFreeObject(body_object);

        return GOOD;   
    }
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
