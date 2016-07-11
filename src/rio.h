#pragma once

#ifndef _RIO_H
#define _RIO_H

#include <unistd.h>
 
namespace VWServer {

const int RIO_BUFSIZE = 8192;

class rio_t {
public:
    int rio_fd;
    int rio_cnt;
    char *rio_bufptr;
    char rio_buf[RIO_BUFSIZE];
};

/*
 * rio包自动处理Unix read函数和Unix write函数的不足值情况
 * 这里使用的带缓冲rio输入函数是线程安全的
 * 使用rio_readlineb和rio_readnb前应先调用rio_readinitb函数
 */
class Rio {
public:
    /**
     * 从位置usrbuf传送n个字节到描述符fd。绝不会返回不足值
     */
    static void readinitb(rio_t *rp, int fd);
    /**
     * 最多读取maxlen-1个字节，余下的一个字符留给结尾的空字符。
     * 超过maxlen-1字节的文本行被截断，并用一个空字符结束
     */
    static ssize_t readlineb(rio_t *rp, void *usrbuf, size_t maxlen);
    /**
     * 从rp最多读n个字节到存储器位置usrbuf
     */
    static ssize_t readnb(rio_t *rp, void *usrbuf, size_t n);
    /**
     * 将surbuf中的前n个字符写到fd中
     */
    static ssize_t writen(int fd, void *usrbuf, size_t n);

private:
	/*
     * rio_read函数是Unix read函数的带缓冲版本
     */
	static ssize_t rRead(rio_t *rp, void *usrbuf, size_t n);
};

}

#endif