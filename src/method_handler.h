#pragma once

#ifndef _METHOD_HANDLER_H
#define _METHOD_HANDLER_H

#include <string>
#include <memory>

namespace VWServer
{

class MethodHandler {
public:
	MethodHandler();
    //factory method
    static std::unique_ptr<MethodHandler> methodClassFactory(const std::string& method, const std::string& uri
        , const std::string& version, const std::string& cgiargs, const std::string& post_content);

    virtual void handle() = 0;

private:

};

class GetStaticHandler: public MethodHandler {
public:
    GetStaticHandler(std::string method, std::string uri);
    void handle();
};

class GetDynamicHandler: public MethodHandler {
public:
    GetDynamicHandler(std::string method, std::string uri, std::string cgiargs);
    void handle();
};

class PostHandler: public MethodHandler {   
public:
    PostHandler(std::string method, std::string uri, std::string post_content);
    void handle();
};

class NothingToDoHandler: public MethodHandler {
public:
	NothingToDoHandler();
    void handle();
};



}

#endif
