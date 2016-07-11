#pragma once

#ifndef _TASK_H
#define _TASK_H

#include "method_handler.h"
#include <memory>
#include <list>
#include <string>

namespace VWServer
{

class Task {
public:
    Task(int fd);
    ~Task();
    void run();
private:
    enum { REQUEST_MAX_SIZE = 1024 };
    int connfd;
    void doit();
    std::string parseUri(std::string& uri);
    void parseHeader(std::list<std::string>& headerList);
    void parsePostContent(std::string& postContent);
};

}

#endif
