#ifndef _SERVER_H
#define _SERVER_H

#include <string>
#include <sys/epoll.h>
class Server {
public:
    Server();
    ~Server();
    void server_error_info(std::string s);
private:
    typedef struct sockaddr SA;
    int listenfd;

    int epollfd;
    enum { MAX_EVENTS = 32 };
    struct epoll_event event;
    struct epoll_event events[MAX_EVENTS];

    void start_server();
    void init_epoll();
    void run();

    void close_server();
    int make_socket_non_blocking(int fd);
};

#endif
