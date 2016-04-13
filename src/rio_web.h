/*
 * rio包自动处理Unix read函数和Unix write函数的不足值情况
 * 这里使用的带缓冲rio输入函数是线程安全的
 * 使用rio_readlineb和rio_readnb前应先调用rio_readinitb函数
 */
#ifndef _RIO_WEB_H
#define _RIO_WEB_H

#include <unistd.h>
#include <memory.h>
#include <cerrno>
#define RIO_BUFSIZE 8192

typedef struct {
	int rio_fd;
	int rio_cnt;
	char *rio_bufptr;
	char rio_buf[RIO_BUFSIZE];
} rio_t;

ssize_t rio_read(rio_t *rp, void *usrbuf, size_t n);

void rio_readinitb(rio_t *rp, int fd);
ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen);
ssize_t rio_readnb(rio_t *rp, void *usrbuf, size_t n);

ssize_t rio_writen(int fd, void *usrbuf, size_t n);


#endif
