#ifndef _TASK_H
#define _TASK_H

#include <string>
#include "methodhandler.h"

class Task {
public:
    Task(int fd);
    ~Task();
    void run();
private:
    enum { REQUEST_MAX_SIZE = 1024 };
    int connfd;
    void doit();
    void parse_uri(std::string &uri, std::string &cgiargs);
    Method_handler *method_class_factory(std::string &method, std::string &uri, std::string &version, std::string &cgiargs, std::string &post_content);
};

#endif
