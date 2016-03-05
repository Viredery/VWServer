#include <stdio.h>
#include <stdlib.h>	/*setenv*/
#include <memory.h>
#include <string.h>
#include <fcntl.h>	/*open*/
#include <unistd.h>	/*access,dup2*/
#include <sys/stat.h>	/*stat*/
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>	/*htons*/
#include <arpa/inet.h>	/*inet_aton*/
#include "rio.h"
extern char **environ;
struct st_request_info {
	char *method;
	char *pathinfo;
	int is_static;
	char *query;
	char *protocal;
	char *path;
	char *file;
	char *physical_path;
};
typedef struct sockaddr SA;
const int LISTENQ = 1024;	/*内核在拒绝连接请求前应放入队列中等待的未完成连接请求的数量*/
const int REQUEST_MAX_SIZE = 1024;
const int FILE_MAX_SIZE = 8192;

int doit(int fd, struct sockaddr_in clientaddr);
int parse_uri(char *buf, char *filename, char *cgiargs);
int proc_request(int fd, struct sockaddr_in clientaddr, struct st_request_info request_info);
int serve_dynamic(int fd, char *filename, char *cgiargs);
int serve_static(int fd, char *filename);
char* strlwr(char *str);

int main()
{
	int listenfd, optval = 1;
	struct sockaddr_in serveraddr;
	if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
		return -1;/*temporary*/

	/*允许套接口和一个已在使用中的地址捆绑*/
	if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int)) < 0)
		return -1;/*temporary*/
/*
	设置127.0.0.1为服务器地址，8000为端口号，用于本机测试
*/
	const char *cp = "127.0.0.1";
	unsigned short int port = 8000;

	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(port);
	if(inet_aton(cp, &serveraddr.sin_addr) == 0)
		return -1;/*temporary*/
	if(bind(listenfd, (SA *) &serveraddr, sizeof(serveraddr)) < 0)
		return -1;/*temporary*/
	if(listen(listenfd, LISTENQ) < 0)
		return -1;/*temporary*/
	while(1)
	{
		int connectfd;
		struct sockaddr_in clientaddr;
		int clientaddrlen = sizeof(clientaddr);
		if ((connectfd = accept(listenfd, (SA *)&clientaddr, &clientaddrlen)) < 0)
			return -1;/*temporary*/
		doit(connectfd, clientaddr);
		close(connectfd);
	}
	return 0;
}
int doit(int fd, struct sockaddr_in clientaddr)
{
	int is_static;
	rio_t rio;
	struct st_request_info request_info;
	char buf[REQUEST_MAX_SIZE];
	char pathinfo[REQUEST_MAX_SIZE], cgiargs[REQUEST_MAX_SIZE], method[REQUEST_MAX_SIZE], uri[REQUEST_MAX_SIZE],version[REQUEST_MAX_SIZE];
	char physical_path[REQUEST_MAX_SIZE], path[REQUEST_MAX_SIZE], file[REQUEST_MAX_SIZE];

	rio_readinitb(&rio, fd); 
//	rio_readnb(&rio, buf, REQUEST_MAX_SIZE);
	rio_readlineb(&rio, buf, REQUEST_MAX_SIZE);
	if(strcmp(strlwr(buf), "\r\n") == 0 || strcmp(strlwr(buf), "\n") == 0)
		return -1;/*temporary*/
	sscanf(buf, "%s %s %s", method, uri, version);
	if(strcmp(strlwr(method), "get") != 0 && strcmp(strlwr(method), "head") != 0)
		return -1;/*temporary*/
/*	while(strcmp(buf, "\r\n"))
	{
		printf("%s",buf);
		rio_readlineb(&rio, buf, REQUEST_MAX_SIZE);
	}
*/

	/*parse*/
	is_static = parse_uri(uri, pathinfo, cgiargs);
	/*set request_info*/
	request_info.method = method;
	request_info.pathinfo = pathinfo;
	request_info.is_static = is_static;
	request_info.query = cgiargs;
	request_info.protocal = version;
	char *posptr = strrchr(pathinfo, '/');
	strncpy(path, pathinfo, posptr - pathinfo + 1);
	path[posptr - pathinfo + 1] = '\0';
	strncpy(file, posptr + 1, REQUEST_MAX_SIZE);
	request_info.path = path;
	request_info.file = file;
	/*
		假设127.0.0.1：8000的文件都放在/home/dingyiran/web_server
	*/
	strncpy(physical_path, "/home/dingyiran/web_server", REQUEST_MAX_SIZE);
	strcat(physical_path, pathinfo);
	request_info.physical_path = physical_path;
	/*添加log系统*/
printf("%s\n%s\n%s\n%s\n%s\n%d\n",request_info.method,request_info.pathinfo,request_info.path,request_info.file,request_info.physical_path,request_info.is_static);
	/*处理*/
	proc_request(fd, clientaddr, request_info); 
}
int parse_uri(char *uri, char *filename, char *cgiargs)
{
	if(!strstr(uri, "cgi-bin"))
	{
		strncpy(cgiargs, "", REQUEST_MAX_SIZE);
		strncpy(filename, uri, REQUEST_MAX_SIZE);
		if(uri[strlen(uri) - 1] == '/')
			strcat(filename, "index.html");
		return 1;
	}else{
		char *p;
		p = index(uri, '?');
		if(p)
		{
			strncpy(cgiargs, p + 1, REQUEST_MAX_SIZE);
			*p = '\0';
		} else {
			strncpy(cgiargs, "", REQUEST_MAX_SIZE);
		}
		strncpy(filename, uri, REQUEST_MAX_SIZE);
		return 0;
	}
}
int proc_request(int fd, struct sockaddr_in clientaddr, struct st_request_info request_info)
{
	struct stat sbuf;
	if(stat(request_info.physical_path, &sbuf) < 0)
		return -1;/*文件不存在，失败，temporary*/
	if(access(request_info.physical_path, R_OK) < 0)
		return -1;/*没有读权限，失败*/
	if(request_info.is_static == 1)
		serve_static(fd, request_info.physical_path);
	else
		serve_dynamic(fd, request_info.physical_path, request_info.query);
}
int serve_static(int fd, char *filename)
{
	char buf[FILE_MAX_SIZE];
	rio_t rio;
	int srcfd;
	if((srcfd = open(filename, O_RDONLY, 0)) < 0)
		return -1;/*temprory*/
	rio_readinitb(&rio, srcfd);
	rio_readnb(&rio, buf, FILE_MAX_SIZE);
	rio_writen(fd, buf, FILE_MAX_SIZE);
}
int serve_dynamic(int fd, char *filename, char *cgiargs)
{
	char buf[REQUEST_MAX_SIZE], *emptylist[] = { NULL };
	sprintf(buf, "HTTP/1.0 200 OK\r\n");
	rio_writen(fd, buf, REQUEST_MAX_SIZE);
	sprintf(buf, "Server:Web Server\r\n");
	rio_writen(fd, buf, REQUEST_MAX_SIZE);
	/*

		待修改

	*/
	if(fork() == 0)
	{
		setenv("QUERY_STRING", cgiargs, 1);
		dup2(fd, STDOUT_FILENO);
		execve(filename, emptylist, environ);
	}
}
char* strlwr(char *str)
{
	if(str == NULL)
		return NULL;
	char *p = str;
	while (*p != '\0')
	{
		if(*p >= 'A' && *p <= 'Z')
			*p = (*p) + 0x20;
		p++;
	}
	return str;
}
