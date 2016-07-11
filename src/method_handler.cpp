#include "method_handler.h"

namespace VWServer
{

MethodHandler::MethodHandler() {
	
}

std::unique_ptr<MethodHandler> MethodHandler::methodClassFactory(const std::string& method
    , const std::string& uri
    , const std::string& version
    , const std::string& cgiargs
    , const std::string& postContent) {

    std::unique_ptr<MethodHandler> ptrHandler;
    if(0 == strcasecmp(method.c_str(), "GET")) {
        if(!cgiargs.empty())
	        ptrHandler.reset(new GetDynamicHandler(method, uri, cgiargs));
	    else
	        ptrHandler.reset(new GetStaticHandler(method, uri));
    } else if(0 == strcasecmp(method.c_str(), "POST"))
        ptrHandler.reset(new PostHandler(method, uri, postContent));
    else
        ptrHandler.reset(new NothingToDoHandler());

    return std::move(ptrHandler);
}


GetStaticHandler::GetStaticHandler(std::string method, std::string uri)
    : MethodHandler() {

}

void GetStaticHandler::handle() {
    //TODO
}

GetDynamicHandler::GetDynamicHandler(std::string method, std::string uri, std::string cgiargs)
    : MethodHandler() {

}

void GetDynamicHandler::handle() {
    //TODO
}

PostHandler::PostHandler(std::string method, std::string uri, std::string post_content)
    : MethodHandler() {

}

void PostHandler::handle() {
    //TODO
}

NothingToDoHandler::NothingToDoHandler()
    : MethodHandler() {

}

void NothingToDoHandler::handle() {
    //TODO
}

}
