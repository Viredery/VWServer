#ifndef _METHOD_HANDLER_H
#define _METHOD_HANDLER_H

#include <string>

class Method_handler {
public:
    virtual void handle();
};

class Get_static_handler: public Method_handler {
public:
    Get_static_handler(std::string method, std::string uri);
    void handle();
};

class Get_dynamic_handler: public Method_handler {
public:
    Get_dynamic_handler(std::string method, std::string uri, std::string cgiargs);
    void handle();
};

class Post_handler: public Method_handler {
public:
    Post_handler(std::string method, std::string uri, std::string post_content);
    void handle();
};

class Nothing_to_do_handler: public Method_handler {
public:
    void handle();
};

#endif
