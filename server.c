#include <stdio.h>
#include <stdlib.h>	/*setenv*/
#include <memory.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>	/*open*/
#include <unistd.h>	/*access,dup2*/
#include <sys/stat.h>	/*stat*/
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>	/*htons*/
#include <arpa/inet.h>	/*inet_aton*/
#include <sys/epoll.h>
#include <fcntl.h> /*fcntl*/
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include "rio_web.h"

#define THREAD_MAX 100
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
const int MAX_EVENTS = 32;	/*内核在拒绝连接请求前应放入队列中等待的未完成连接请求的数量*/
const int REQUEST_MAX_SIZE = 1024;
const int FILE_MAX_SIZE = 8192;
const char SERVER_NAME[] = "Web Server";
const char LOG_FILE[] = "proxy.log";
sem_t log_mutex;
pthread_t pool_tid[THREAD_MAX];//线程ID
int pool_thread_para[THREAD_MAX][8];//线程参数
pthread_mutex_t pool_mutex[THREAD_MAX];//线程锁
int thread_pool_init();
void *thread_func(int *thread_parameter);


int doit(int fd, int logfd);
int parse_uri(char *buf, char *filename, char *cgiargs);
int proc_request(int fd, struct st_request_info request_info);
int serve_dynamic(int fd, char *filename, char *cgiargs);
int serve_static(int fd, char *filename, int file_size);
char* strlwr(char *str);
void send_error(int fd, int status, char *title, char *text);
static int send_headers(int fd, int status, char *title, char *mime_type, int length);
void mime_content_type( const char *name, char *ret );
int log_file_init();
void write_log_file(int logfd, char *method, char *uri);
int make_socket_non_blocking(int fd);


int main()
{
	int epollfd;
	int listenfd, optval = 1;
	struct sockaddr_in serveraddr;
	struct epoll_event event;
	struct epoll_event *events;

//	signal(SIGINT, server_stop);

	/*initialize the log file*/
	int logfd;
	if((logfd = log_file_init()) < 0)
	{
		perror("Server does not found log file");
		exit(1);
	}
	/*initialize the thread pool*/
	if(thread_pool_init() < 0)
	{
		perror("Failed to create thread pool");
		exit(1);
	}

	if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
	{
		perror("Failed to create server socket");
		exit(1);
	}

	/*允许套接口和一个已在使用中的地址捆绑，服务器快速重启*/
	if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int)) < 0)
	{
		perror("Failed to set SO_REUSEADDR");
		exit(1);
	}
/*
	设置127.0.0.1为服务器地址，8000为端口号，用于本机测试
*/
	const char *cp = "127.0.0.1";
	unsigned short int port = 8000;

	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(port);
	if(inet_aton(cp, &serveraddr.sin_addr) == 0)
	{
		perror("Failed to set struct sockaddr_in");
		exit(1);
	}
	if(bind(listenfd, (SA *) &serveraddr, sizeof(serveraddr)) < 0)
	{
		perror("Failed to bind the server socket");
		exit(1);
	}
	if(make_socket_non_blocking(listenfd) < 0)
	{
		perror("Failed to set non_blocking");
		exit(1);
	}
	if(listen(listenfd, SOMAXCONN) < 0)
	{
		perror("Failed to listen on server socket");
		exit(1);
	}
	if((epollfd = epoll_create(MAX_EVENTS + 1)) < 0)
	{
		perror("Failed to create epollfd");
		exit(1);
	}
	event.data.fd = listenfd;
	event.events = EPOLLIN | EPOLLET;
	if(epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &event) < 0)
	{
		perror("Failed to create epollfd");
		exit(1);
	}
	events = calloc(MAX_EVENTS, sizeof(event));
	

	while(1)
	{
		int n, i, j;
		n = epoll_wait(epollfd, events, MAX_EVENTS, -1);
		for(i = 0; i != n; i++)
		{
			if((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN)))
			{
				close(events[i].data.fd); 
				continue;
			} else if(events[i].data.fd == listenfd) {
				while(1)
				{
					int connectfd;
					struct sockaddr_in clientaddr;
					int clientaddrlen = sizeof(clientaddr);
					if((connectfd = accept(listenfd, (SA *)&clientaddr, &clientaddrlen)) < 0)
					{
						if(errno == EAGAIN || errno == EWOULDBLOCK)
							/* We have processed all incoming connections. */  
							break;
						perror("Failed to accept client connection");
						break;
					}
					if(make_socket_non_blocking(connectfd) < 0)
					{
						perror("Failed to set non_blocking");
						exit(1);
					}
					event.data.fd = connectfd;
					event.events = EPOLLIN | EPOLLET;
					if(epoll_ctl(epollfd, EPOLL_CTL_ADD, connectfd, &event) < 0)
					{
						perror("Failed to create epollfd");
						exit(1);
					}
				}
			} else {
				/*查询空闲线程*/
				for(j = 0; j < THREAD_MAX; j++)
				{
					if(pool_thread_para[j][0] == 0) break; 
				}
				if(j >= THREAD_MAX)
				{
					send_error(events[i].data.fd, 503, "Service Unavailable", "Server has no usable threads.");
					continue; 
				}
				/*复制有关参数*/
				pool_thread_para[j][0] = 1;//设置活动标志为"活动"
				pool_thread_para[j][1] = events[i].data.fd;//客户端连接
				pool_thread_para[j][2] = logfd;
				/*线程解锁*/
				pthread_mutex_unlock(pool_mutex + j); 
			}

		}
	}
	free(events);
	close(logfd);
	close(listenfd);
	return 0;
}
int doit(int fd, int logfd)
{
	int is_static;
	rio_t rio;
	struct st_request_info request_info;
	char buf[REQUEST_MAX_SIZE];
	char pathinfo[REQUEST_MAX_SIZE], cgiargs[REQUEST_MAX_SIZE], method[REQUEST_MAX_SIZE], uri[REQUEST_MAX_SIZE],version[REQUEST_MAX_SIZE];
	char physical_path[REQUEST_MAX_SIZE], path[REQUEST_MAX_SIZE], file[REQUEST_MAX_SIZE];

	rio_readinitb(&rio, fd); 
	if (rio_readlineb(&rio, buf, REQUEST_MAX_SIZE) < 0)
		send_error(fd, 500, "Internal Server Error", "Client request not success.");
	else if(strcmp(strlwr(buf), "\r\n") == 0 || strcmp(strlwr(buf), "\n") == 0)
		send_error(fd, 400, "Bad Request", "Client request empty.");
	else {
		sscanf(buf, "%s %s %s", method, uri, version);
		if(strcmp(strlwr(method), "get") != 0 && strcmp(strlwr(method), "head") != 0)
			send_error(fd, 501, "Not Implemented", "That method is not implemented.");
		else {
			/*ignore header*/
			while(strcmp(buf, "\r\n")){
				rio_readlineb(&rio, buf, REQUEST_MAX_SIZE);
			}
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
			/*写入日志文件*/
			write_log_file(logfd, method, uri);
			/*处理*/
			proc_request(fd, request_info);
		} 
	}
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
int proc_request(int fd, struct st_request_info request_info)
{
	struct stat sbuf;
	if(stat(request_info.physical_path, &sbuf) < 0 || !S_ISREG(sbuf.st_mode))
		send_error(fd, 404, "Not Found", "Server not found this file." );
	else if(access(request_info.physical_path, R_OK) < 0)
		send_error(fd, 403, "Forbidden", "Client not read this file.");
	else if(request_info.is_static == 1)
		serve_static(fd, request_info.physical_path, sbuf.st_size);
	else
		serve_dynamic(fd, request_info.physical_path, request_info.query);
}
int serve_static(int fd, char *filename, int file_size)
{
	char buf[REQUEST_MAX_SIZE], ret[REQUEST_MAX_SIZE], content[FILE_MAX_SIZE];
	rio_t rio;
	int srcfd;

	if((srcfd = open(filename, O_RDONLY, 0)) < 0)
		send_error(fd, 403, "Forbidden", "Client not read this file.");
	else {
		memset(ret, 0, strlen(ret));
		mime_content_type(filename, ret);
		send_headers(fd, 200, "OK", ret, file_size);
		memset(content, 0, strlen(content));
		rio_readinitb(&rio, srcfd);
		rio_read(&rio, content, FILE_MAX_SIZE);
		write(fd, content, strlen(content));
		close(srcfd);
	}
}
int serve_dynamic(int fd, char *filename, char *cgiargs)
{
	char buf[REQUEST_MAX_SIZE], *emptylist[] = { NULL };
	sprintf(buf, "HTTP/1.0 200 OK\r\n");
	rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Server:SERVER_NAME\r\n");
	rio_writen(fd, buf, strlen(buf));
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
void send_error(int fd, int status, char *title, char *text)
{
	char buf[REQUEST_MAX_SIZE], buf_all[REQUEST_MAX_SIZE];
	memset(buf_all, 0, strlen(buf_all));

	/* Send http header */
	send_headers(fd, status, title, "text/html", -1);
	/* Send html page */
	memset(buf, 0, strlen(buf));
	sprintf(buf, "<html>\n<head>\n<title>%d %s - %s</title>\n</head>\n<body>\n<h2>%d %s</h2>\n", status, title, SERVER_NAME, status, title);
	strcat(buf_all, buf);

	memset(buf, 0, strlen(buf));
	sprintf(buf, "%s\n", text);
	strcat(buf_all, buf);

	memset(buf, 0, strlen(buf));
	sprintf(buf, "\n<br /><br /><hr />\n<address>%s</address>\n</body>\n</html>\n", SERVER_NAME);
	strcat(buf_all, buf);

	/* Write client socket */
	rio_writen(fd, buf_all, strlen(buf_all));
	close(fd);
}
static int send_headers(int fd, int status, char *title, char *mime_type, int length)
{
	char buf[REQUEST_MAX_SIZE];

	/* Make http head information */
	memset(buf, 0, strlen(buf));
	sprintf(buf, "HTTP/1.0 %d %s\r\n", status, title);
	rio_writen(fd, buf, strlen(buf));

	memset(buf, 0, strlen(buf));
	sprintf(buf, "Server: %s\r\n", SERVER_NAME);
	rio_writen(fd, buf, strlen(buf));

	if(mime_type != (char*)0)
	{
		memset(buf, 0, strlen(buf));
		sprintf(buf, "Content-Type: %s\r\n", mime_type);
		rio_writen(fd, buf, strlen(buf));
	}
	if(length >= 0)
	{
		memset(buf, 0, strlen(buf));
		sprintf(buf, "Content-Length: %d\r\n", length);        
		rio_writen(fd, buf, strlen(buf));
	}
	memset(buf, 0, strlen(buf));
	sprintf(buf, "Connection: close\r\n\r\n");
	rio_writen(fd, buf, strlen(buf));

	return 0;
}
void mime_content_type(const char *name, char *ret)
{
	char *dot, *buf; 

	dot = strrchr(name, '.'); 
	if(dot == NULL) {
		buf = "text/plain";
	} else if(strcmp(dot, ".txt") == 0) {
		buf = "text/plain";
	} else if(strcmp( dot, ".css") == 0) {
		buf = "text/css";
	} else if( strcmp( dot, ".js" ) == 0) {
		buf = "text/javascript";
	} else if(strcmp(dot, ".xml") == 0 || strcmp(dot, ".xsl") == 0) {
		buf = "text/xml";
	} else if(strcmp(dot, ".xhtm") == 0 || strcmp(dot, ".xhtml") == 0 || strcmp(dot, ".xht") == 0) {
		buf = "application/xhtml+xml";
	} else if(strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0 || strcmp(dot, ".shtml") == 0 || strcmp(dot, ".hts") == 0) {
		buf = "text/html";
	/* Images */
	} else if(strcmp(dot, ".gif") == 0) {
		buf = "image/gif";
	} else if(strcmp(dot, ".png") == 0) {
		buf = "image/png";
	} else if(strcmp(dot, ".bmp") == 0) {
		buf = "application/x-MS-bmp";
	} else if(strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0 || strcmp(dot, ".jpe") == 0 || strcmp(dot, ".jpz") == 0) {
		buf = "image/jpeg";
	/* Audio & Video */
	} else if(strcmp(dot, ".wav") == 0) {
		buf = "audio/wav";
	} else if(strcmp(dot, ".wma") == 0) {
		buf = "audio/x-ms-wma";
	} else if(strcmp(dot, ".wmv") == 0) {
		buf = "audio/x-ms-wmv";
	} else if(strcmp(dot, ".au") == 0 || strcmp(dot, ".snd") == 0) {
		buf = "audio/basic";
	} else if(strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0) {
		buf = "audio/midi";
	} else if(strcmp(dot, ".mp3") == 0 || strcmp(dot, ".mp2") == 0) {
		buf = "audio/x-mpeg";
	} else if(strcmp(dot, ".rm") == 0 || strcmp(dot, ".rmvb") == 0 || strcmp(dot, ".rmm") == 0) {
		buf = "audio/x-pn-realaudio";
	} else if(strcmp(dot, ".avi") == 0) {
		buf = "video/x-msvideo";
	} else if(strcmp(dot, ".3gp") == 0) {
		buf = "video/3gpp";
	} else if(strcmp(dot, ".mov") == 0) {
		buf = "video/quicktime";
	} else if(strcmp(dot, ".wmx") == 0) {
		buf = "video/x-ms-wmx";
	} else if(strcmp(dot, ".asf") == 0  || strcmp(dot, ".asx") == 0) {
		buf = "video/x-ms-asf";
	} else if(strcmp(dot, ".mp4") == 0 || strcmp(dot, ".mpg4") == 0) {
		buf = "video/mp4";
	} else if(strcmp(dot, ".mpe") == 0 || strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpg") == 0 || strcmp(dot, ".mpga") == 0) {
		buf = "video/mpeg";
	/* Documents */
	} else if(strcmp(dot, ".pdf") == 0) {
		buf = "application/pdf";
	} else if(strcmp(dot, ".rtf") == 0) {
		buf = "application/rtf";
	} else if(strcmp(dot, ".doc") == 0 || strcmp(dot, ".dot") == 0) {
		buf = "application/msword";
	} else if(strcmp(dot, ".xls") == 0 || strcmp(dot, ".xla") == 0) {
		buf = "application/msexcel";
	} else if(strcmp(dot, ".hlp") == 0 || strcmp(dot, ".chm") == 0) {
		buf = "application/mshelp";
	} else if(strcmp(dot, ".swf") == 0 || strcmp(dot, ".swfl") == 0 || strcmp(dot, ".cab") == 0) {
		buf = "application/x-shockwave-flash";
	} else if(strcmp(dot, ".ppt") == 0 || strcmp(dot, ".ppz") == 0 || strcmp(dot, ".pps") == 0 || strcmp(dot, ".pot") == 0) {
		buf = "application/mspowerpoint";
	/* Binary & Packages */
	} else if(strcmp(dot, ".zip") == 0) {
		buf = "application/zip";
	} else if(strcmp(dot, ".rar") == 0) {
		buf = "application/x-rar-compressed";
	} else if(strcmp(dot, ".gz") == 0 ) {
		buf = "application/x-gzip";
	} else if(strcmp(dot, ".jar") == 0) {
		buf = "application/java-archive";
	} else if(strcmp(dot, ".tgz") == 0 || strcmp(dot, ".tar") == 0) {
 		buf = "application/x-tar";
	} else {
		buf = "application/octet-stream";
	}
	strcat(ret, buf);
}

int log_file_init()
{
	int fd;
	if((fd = open(LOG_FILE, O_WRONLY, O_APPEND)) < 0)
		return -1;
	if(sem_init(&log_mutex, 0, 1) < 0)
		return -1;
	return fd;
}
void write_log_file(int logfd, char *method, char *uri)
{
	time_t now;
	char clientlog[FILE_MAX_SIZE], timebuf[FILE_MAX_SIZE];
	memset(timebuf, 0, strlen(timebuf));
	now = time((time_t*) 0);
	strftime(timebuf, sizeof(timebuf), "%a, %d %b %Y %H:%M:%S GMT: ", gmtime(&now));
	memset(clientlog, 0, strlen(clientlog));
	strcat(clientlog, timebuf);
	strcat(clientlog, method);
	strcat(clientlog, " ");
	strcat(clientlog, uri);
	strcat(clientlog, "\n");
	sem_wait(&log_mutex);
	lseek(logfd, 0, SEEK_END);
	rio_writen(logfd, clientlog, strlen(clientlog));
	sem_post(&log_mutex);
}

int make_socket_non_blocking(int fd)
{
	int flags, s;
  
	if((flags = fcntl(fd, F_GETFL, 0)) == -1) 
		return -1;
	flags |= O_NONBLOCK;
	if(fcntl(fd, F_SETFL, flags) == -1) 
		return -1;
	return 0;
}
int thread_pool_init()
{
	int i;
	for(i = 0; i != THREAD_MAX; i++)
	{
		pool_thread_para[i][0] = 0;//表示空闲
		pool_thread_para[i][7] = i;//线程池索引
		pthread_mutex_lock(pool_mutex + i);
	}
	for(i = 0; i != THREAD_MAX; i++)
		if(pthread_create(pool_tid + i, NULL, (void* (*)(void *))thread_func, (void *)pool_thread_para[i]) != 0)
			return -1;
	return 0;
}
void *thread_func(int *thread_parameter)
{
	int pool_index = thread_parameter[7];
	pthread_detach(pthread_self());
	while(1)
	{
		pthread_mutex_lock(pool_mutex + pool_index);
		int connectfd = thread_parameter[1];

		doit(connectfd, thread_parameter[2]);
		close(connectfd);
		thread_parameter[0] = 0;
	}
	return NULL;
}
