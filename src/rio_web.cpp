#include "rio_web.h"
/*
 * 从位置usrbuf传送n个字节到描述符fd。绝不会返回不足值
 */
ssize_t rio_writen(int fd, void *usrbuf, size_t n)
{
    size_t nleft = n;
    ssize_t nwritten;
    char *bufp = static_cast<char *>(usrbuf);

    while(nleft > 0) {
        if((nwritten = write(fd, bufp, nleft)) <= 0) {
	if(errno == EINTR)	/*当写的过程中遇到了中断*/
	    nwritten = 0;
	else	/*其他问题*/
	    return -1;
	}
	nleft -= nwritten;
	bufp += nwritten;
    }
    return n;
}
/*
 * rio_readinitb函数将rio与文件描述符结合起来，完成带缓冲读的初始化
 */
void rio_readinitb(rio_t *rp, int fd)
{
    rp->rio_fd = fd;
    rp->rio_cnt = 0;
    rp->rio_bufptr = rp->rio_buf;
}
/*
 * rio_read函数是Unix read函数的带缓冲版本
 */
ssize_t rio_read(rio_t *rp, void *usrbuf, size_t n)
{
    int cnt;
    while(rp->rio_cnt <= 0) {
        rp->rio_cnt = read(rp->rio_fd, rp->rio_buf, sizeof(rp->rio_buf));
	if(rp->rio_cnt < 0) {
	    if(errno != EINTR)
	        return -1;
	} else if(rp->rio_cnt == 0)
	    return 0;
	else
	    rp->rio_bufptr = rp->rio_buf;
    }
    cnt = n;
    if(rp->rio_cnt < n)
        cnt = rp->rio_cnt;
    memcpy(usrbuf, rp->rio_bufptr, cnt);
    rp->rio_bufptr += cnt;
    rp->rio_cnt -= cnt;
    return cnt;
}
/*
 * 最多读取maxlen-1个字节，余下的一个字符留给结尾的空字符。
 * 超过maxlen-1字节的文本行被截断，并用一个空字符结束
 */
ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen)
{
    int n, rc;
    char c, *bufp = static_cast<char *>(usrbuf);

    for(n = 1; n < maxlen; n++) {
        if((rc = rio_read(rp, &c, 1)) == 1) {
	    *bufp++ = c;
	    if(c == '\n')
	        break;
	    } else if(rc == 0) {	/*EOF情况*/
	        if(n == 1)
		    return 0;
		else
		    break;
	    } else
	        return -1;
    }
    *bufp = 0;
    return n;
}
/*
 * 从rp最多读n个字节到存储器位置usrbuf
 */
ssize_t rio_readnb(rio_t *rp, void *usrbuf, size_t n)
{
    ssize_t nleft = n;
    ssize_t nread;
    char *bufp = static_cast<char *>(usrbuf);
    while(nleft > 0) {
        if((nread = rio_read(rp, bufp, nleft)) < 0) {
	    if(errno == EINTR)
	        nread = 0;
	    else
	        return -1;
	} else if(nread == 0)
	    break;
	nleft -= nread;
	bufp += nread;
    }
    return (n - nleft);
}
