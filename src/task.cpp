#include "task.h"
#include "rio.h"
#include "vlogger.h"
#include <cstring>
#include <utility>

namespace VWServer
{

Task::Task(int fd) {
    connfd = fd;
}

Task::~Task() {
}

void Task::run() {
    doit();
}

void Task::doit() {
    rio_t rio;
    char buf[REQUEST_MAX_SIZE];
    Rio::readinitb(&rio, connfd);
    //parse request line
    if(Rio::readlineb(&rio, buf, REQUEST_MAX_SIZE) < 0) {
        VLogger::getInstance()->error("Failed to recv the request line.");
	    return;
    }
    std::string requestLine(buf, buf + strlen(buf));
    if(requestLine.compare("\r\n") == 0) {
        VLogger::getInstance()->error("Request line is empty.");
	    return;
    }
    std::string::size_type firstSpace = requestLine.find(' ', 0);
    std::string::size_type secondSpace = requestLine.find(' ', firstSpace + 1);
    if (std::string::npos == firstSpace || std::string::npos == secondSpace) {
        VLogger::getInstance()->error("A formal error occurs in the request line.");
        return;
    }
    std::string method(requestLine, 0, firstSpace);
    std::string uri(requestLine, firstSpace + 1, secondSpace - firstSpace - 1);
    std::string version(requestLine, secondSpace + 1, requestLine.size() - secondSpace - 3);
    //be sure whether this is a cgi request
    std::string cgiargs = parseUri(uri);
    int lineLength = 0;
    //parse header content
    std::list<std::string> headerlist;
    while ((lineLength = Rio::readlineb(&rio, buf, REQUEST_MAX_SIZE)) > 0 && strcasecmp(buf, "\r\n") != 0) {
        std::string header(buf, lineLength - 2);
        headerlist.push_back(header);
    }
    parseHeader(headerlist);
    //be sure whether this is a post request
    std::string postContent;
    while ((lineLength = Rio::readlineb(&rio, buf, REQUEST_MAX_SIZE)) > 0) {
        postContent.append(buf, lineLength);
    }
    parsePostContent(postContent);

    //deal with request
    std::unique_ptr<MethodHandler> ptrHandler = MethodHandler::methodClassFactory(method, uri, version, cgiargs, postContent);
    if (ptrHandler.get()) {
        ptrHandler->handle();
        //release memory
        ptrHandler.reset();
    }
    
}

std::string Task::parseUri(std::string& uri) {
    std::string::size_type pos = uri.find('?');
    std::string::size_type length = uri.size();
    std::string cgiargs;
    if(pos != std::string::npos) {
        cgiargs = uri.substr(pos + 1, length - pos - 1);
	    uri.erase(pos, length - pos);
    }
    return cgiargs;
}

void Task::parseHeader(std::list<std::string>& headerList) {
    //EXTEND in the future
}

void Task::parsePostContent(std::string& postContent) {
    //EXTEND in the future
}

}
int main()
{}