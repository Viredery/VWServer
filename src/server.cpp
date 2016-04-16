#include "server.h"
#include "thread_pool.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <iostream>
#include <fcntl.h>
#include <errno.h>
using namespace std;


Server::Server(int thread_amount) {
    start_server();
    init_epoll();
    if(thread_amount < 1)
        server_error_info("Failed to initialize the thread pool.");
    Thread_pool::get_thread_pool(thread_amount);
}

Server::~Server() {
}

void Server::start_server() {
    if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        server_error_info("Failed to create server socket.");
    int optval = 1;
    if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int)) < 0)
        server_error_info("Failed to set SO_REUSEADDR.");
    //127.0.0.1:8000 for test
    const char *ipaddr = "127.0.0.1";
    unsigned short int port = 8000;
    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);
    if(inet_aton(ipaddr, &serveraddr.sin_addr) == 0)
        server_error_info("Failed to set struct sockaddr_in.");
    if(bind(listenfd, (SA *) &serveraddr, sizeof(serveraddr)) < 0)
        server_error_info("Failed to bind the server socket.");
    if(make_socket_non_blocking(listenfd) < 0)
        server_error_info("Failed to set socket nonblock.");
    if(listen(listenfd, SOMAXCONN) < 0)
        server_error_info("Failed to listen on server socket.");
}

void Server::server_error_info(string s) {
    cerr << s << endl;
    exit(1);
}

void Server::close_server() {
    close(listenfd);
}

int Server::make_socket_non_blocking(int fd) {
    int flags;
    if((flags = fcntl(fd, F_GETFL, 0)) == -1)
        return -1;
    flags |= O_NONBLOCK;
    if(fcntl(fd, F_SETFL, flags) == -1)
        return -1;
    return 0;
}

void Server::init_epoll() {
    if((epollfd = epoll_create(MAX_EVENTS + 1)) < 0)
        server_error_info("Failed to create epollfd");
    event.data.fd = listenfd;
    event.events = EPOLLIN | EPOLLET;
    if(epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &event) < 0)
        server_error_info("Failed to create epollfd");
}

void Server::run() {
    while(true) {
        int n, i, j;
	n = epoll_wait(epollfd, events, MAX_EVENTS, -1);
	for(i = 0; i != n; i++) {
	    if((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN))) {
	        close(events[i].data.fd);
		continue;
	    } else if(events[i].data.fd == listenfd) {
	        //maybe more than one connect request
	        while(true) {
		    int connfd;
		    if((connfd = accept(listenfd, NULL, NULL)) < 0) {
		        if(errno == EAGAIN || errno == EWOULDBLOCK)
			    //all incomint connections have been processed
			    break;
			//should better change this function[not function in stdio.h]
			perror("Failed to accept client connection");
			break;
		    }
		    if(make_socket_non_blocking(connfd) < 0)
		        //need to change[should not exit]
		        server_error_info("Failed to set non_blocking");
		    event.data.fd = connfd;
		    event.events = EPOLLIN | EPOLLET;
		    if(epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, &event) < 0)
		        //need to change[should not exit]
			server_error_info("Failed to create epollfd");
		}
	    } else {
	        //deal with client requests
		Task *task = new Task(events[i].data.fd);
		Thread_pool::get_thread_pool()->add_task(task);
	    }
	}
    }
}
