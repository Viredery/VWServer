#include "task.h"
#include "rio_web.h"
#include <cstring>
using namespace std;

Task::Task(int fd) {
    connfd = fd;
}

Task::~Task() {
}

void Task::run() {
    doit();
}

void Task::doit() {
    int is_static;
    rio_t rio;
    char buf[REQUEST_MAX_SIZE];
    rio_readinitb(&rio, connfd);
    if(rio_readlineb(&rio, buf, REQUEST_MAX_SIZE) < 0) {
        //send error info
	return;
    }
    string request_line(buf, buf + strlen(buf));
    if(request_line.compare("\r\n") == 0) {
        //send error info
	return;
    }
    string::size_type first_space = request_line.find(' ', 0);
    string::size_type second_space = request_line.find(' ', first_space + 1);
    string method(request_line, 0, first_space);
    string uri(request_line, first_space + 1, second_space - first_space - 1);
    string version(request_line, second_space + 1, request_line.size() - second_space - 3);
    //be sure whether this is a cgi request
    string cgiargs;
    parse_uri(uri, cgiargs);
    //be sure whether this is a post request
    string post_content;
    /*
     *
     * save post content
     * not finish
     *
     *
     */
    Method_handler *m = method_class_factory(method, uri, version, cgiargs, post_content);
    m->handle();
    delete m;
}

void Task::parse_uri(string &uri, string &cgiargs) {
    string::size_type pos = uri.find('?');
    string::size_type amount = uri.size();
    if(pos != string::npos) {
        cgiargs.append(uri, pos + 1, amount - pos - 1);
	uri.erase(pos, amount - pos);
    } else {
        if(uri[amount - 1] == '/')
	    uri.append("index.html");
    }
}

Method_handler *Task::method_class_factory(string &method, string &uri, string &version, string &cgiargs, string &post_content) {
    Method_handler *m;
    if(method.compare("GET") == 0 || method.compare("get") == 0) {
        if(!cgiargs.empty())
	    m = new Get_dynamic_handler(method, uri, cgiargs);
	else
	    m = new Get_static_handler(method, uri);
    } else if(method.compare("POST") == 0 || method.compare("post") == 0)
        m = new Post_handler(method, uri, post_content);
    else
        m = new Nothing_to_do_handler();
}
