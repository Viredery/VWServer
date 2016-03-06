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
const char SERVER_NAME[] = "Web Server";

int doit(int fd, struct sockaddr_in clientaddr);
int parse_uri(char *buf, char *filename, char *cgiargs);
int proc_request(int fd, struct sockaddr_in clientaddr, struct st_request_info request_info);
int serve_dynamic(int fd, char *filename, char *cgiargs);
int serve_static(int fd, char *filename, int file_size);
char* strlwr(char *str);
void send_error(int fd, int status, char *title, char *extra_header, char *text);
static int send_headers(int fd, int status, char *title, char *extra_header, char *mime_type, off_t length);
void mime_content_type( const char *name, char *ret );

int main()
{
	int listenfd, optval = 1;
	struct sockaddr_in serveraddr;
	if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
	{
		perror("Failed to create server socket");
		exit(1);
	}

	/*允许套接口和一个已在使用中的地址捆绑*/
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
	if(listen(listenfd, LISTENQ) < 0)
	{
		perror("Failed to listen on server socket");
		exit(1);
	}
	while(1)
	{
		int connectfd;
		struct sockaddr_in clientaddr;
		int clientaddrlen = sizeof(clientaddr);
		if ((connectfd = accept(listenfd, (SA *)&clientaddr, &clientaddrlen)) < 0)
		{
			perror("Failed to accept client connection");
			exit(1);
		}
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
	if (rio_readlineb(&rio, buf, REQUEST_MAX_SIZE) < 0)
		send_error(fd, 500, "Internal Server Error", "", "Client request not success.");
	if(strcmp(strlwr(buf), "\r\n") == 0 || strcmp(strlwr(buf), "\n") == 0)
		send_error(fd, 400, "Bad Request", "", "Client request empty.");
	sscanf(buf, "%s %s %s", method, uri, version);
	if(strcmp(strlwr(method), "get") != 0 && strcmp(strlwr(method), "head") != 0)
		send_error(fd, 501, "Not Implemented", "", "That method is not implemented.");
	/*ignore header*/
	while(strcmp(buf, "\r\n"))
		rio_readlineb(&rio, buf, REQUEST_MAX_SIZE);
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
		send_error(fd, 404, " Not Found", "", "Server not found this file." );
	if(access(request_info.physical_path, R_OK) < 0)
		send_error(fd, 403, "Forbidden", "", "Client not read this file.");
	if(request_info.is_static == 1)
		serve_static(fd, request_info.physical_path, sbuf.st_size);
	else
		serve_dynamic(fd, request_info.physical_path, request_info.query);
}
int serve_static(int fd, char *filename, int file_size)
{
	char buf[FILE_MAX_SIZE], ret[FILE_MAX_SIZE];
	rio_t rio;
	int srcfd;
	if((srcfd = open(filename, O_RDONLY, 0)) < 0)
		send_error(fd, 403, "Forbidden", "", "Client not read this file.");
	rio_readinitb(&rio, srcfd);
	rio_readnb(&rio, buf, FILE_MAX_SIZE);
/*待修改*/
	mime_content_type(filename, ret);
	send_headers(fd, 200, "OK", "", ret, file_size);
	rio_writen(fd, buf, strlen(buf));
}
int serve_dynamic(int fd, char *filename, char *cgiargs)
{
	char buf[REQUEST_MAX_SIZE], *emptylist[] = { NULL };
	sprintf(buf, "HTTP/1.0 200 OK\r\n");
	rio_writen(fd, buf, REQUEST_MAX_SIZE);
	sprintf(buf, "Server:SERVER_NAME\r\n");
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
void send_error(int fd, int status, char *title, char *extra_header, char *text)
{
	char buf[REQUEST_MAX_SIZE], buf_all[REQUEST_MAX_SIZE];

	/* Send http header */
	send_headers(fd, status, title, extra_header, "text/html", -1);

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
	exit(1);
}
static int send_headers(int fd, int status, char *title, char *extra_header, char *mime_type, off_t length)
{
    time_t now;
    char timebuf[100], buf[REQUEST_MAX_SIZE], buf_all[REQUEST_MAX_SIZE], log[8];

	/* Make http head information */
    memset(buf, 0, strlen(buf));
    sprintf(buf, "%s %d %s\r\n", "HTTP/1.0", status, title);
    strcat(buf_all, buf);

    memset(buf, 0, strlen(buf));
    sprintf(buf, "Server: %s\r\n", SERVER_NAME);
    strcat(buf_all, buf);

    now = time( (time_t*) 0 );
    strftime( timebuf, sizeof(timebuf), "%a, %d %b %Y %H:%M:%S GMT", gmtime( &now ) );
    memset(buf, 0, strlen(buf));
    sprintf(buf, "Date: %s\r\n", timebuf);
    strcat(buf_all, buf);

    if (extra_header != (char*)0){
        memset(buf, 0, strlen(buf));
        sprintf(buf, "%s\r\n", extra_header);
        strcat(buf_all, buf);
    }
    if (mime_type != (char*)0){
        memset(buf, 0, strlen(buf));
        sprintf(buf, "Content-Type: %s\r\n", mime_type);
        strcat(buf_all, buf);
    }
    if (length >= 0){
        memset(buf, 0, strlen(buf));
        sprintf(buf, "Content-Length: %lld\r\n", (int64_t)length);        
        strcat(buf_all, buf);
    }
    memset(buf, 0, strlen(buf));
    sprintf(buf, "Connection: close\r\n\r\n");
    strcat(buf_all, buf);
	/* Write http header to client socket */
    rio_writen(fd, buf_all, strlen(buf_all));

	return 0;
}
void mime_content_type( const char *name, char *ret )
{
    char *dot, *buf; 

    dot = strrchr(name, '.'); 
    if(dot == NULL){
	buf = "text/plain";
    } else if ( strcmp(dot, ".txt") == 0 ){
        buf = "text/plain";
    } else if ( strcmp( dot, ".css" ) == 0 ){
        buf = "text/css";
    } else if ( strcmp( dot, ".js" ) == 0 ){
        buf = "text/javascript";
    } else if ( strcmp(dot, ".xml") == 0 || strcmp(dot, ".xsl") == 0 ){
        buf = "text/xml";
    } else if ( strcmp(dot, ".xhtm") == 0 || strcmp(dot, ".xhtml") == 0 || strcmp(dot, ".xht") == 0 ){
        buf = "application/xhtml+xml";
    } else if ( strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0 || strcmp(dot, ".shtml") == 0 || strcmp(dot, ".hts") == 0 ){
        buf = "text/html";

	/* Images */
    } else if ( strcmp( dot, ".gif" ) == 0 ){
        buf = "image/gif";
    } else if ( strcmp( dot, ".png" ) == 0 ){
        buf = "image/png";
    } else if ( strcmp( dot, ".bmp" ) == 0 ){
        buf = "application/x-MS-bmp";
    } else if ( strcmp( dot, ".jpg" ) == 0 || strcmp( dot, ".jpeg" ) == 0 || strcmp( dot, ".jpe" ) == 0 || strcmp( dot, ".jpz" ) == 0 ){
        buf = "image/jpeg";

	/* Audio & Video */
    } else if ( strcmp( dot, ".wav" ) == 0 ){
        buf = "audio/wav";
    } else if ( strcmp( dot, ".wma" ) == 0 ){
        buf = "audio/x-ms-wma";
    } else if ( strcmp( dot, ".wmv" ) == 0 ){
        buf = "audio/x-ms-wmv";
    } else if ( strcmp( dot, ".au" ) == 0 || strcmp( dot, ".snd" ) == 0 ){
        buf = "audio/basic";
    } else if ( strcmp( dot, ".midi" ) == 0 || strcmp( dot, ".mid" ) == 0 ){
        buf = "audio/midi";
    } else if ( strcmp( dot, ".mp3" ) == 0 || strcmp( dot, ".mp2" ) == 0 ){
        buf = "audio/x-mpeg";
	} else if ( strcmp( dot, ".rm" ) == 0  || strcmp( dot, ".rmvb" ) == 0 || strcmp( dot, ".rmm" ) == 0 ){
        buf = "audio/x-pn-realaudio";
    } else if ( strcmp( dot, ".avi" ) == 0 ){
        buf = "video/x-msvideo";
    } else if ( strcmp( dot, ".3gp" ) == 0 ){
        buf = "video/3gpp";
    } else if ( strcmp( dot, ".mov" ) == 0 ){
        buf = "video/quicktime";
    } else if ( strcmp( dot, ".wmx" ) == 0 ){
        buf = "video/x-ms-wmx";
	} else if ( strcmp( dot, ".asf" ) == 0  || strcmp( dot, ".asx" ) == 0 ){
        buf = "video/x-ms-asf";
    } else if ( strcmp( dot, ".mp4" ) == 0 || strcmp( dot, ".mpg4" ) == 0 ){
        buf = "video/mp4";
	} else if ( strcmp( dot, ".mpe" ) == 0  || strcmp( dot, ".mpeg" ) == 0 || strcmp( dot, ".mpg" ) == 0 || strcmp( dot, ".mpga" ) == 0 ){
        buf = "video/mpeg";

	/* Documents */
    } else if ( strcmp( dot, ".pdf" ) == 0 ){
        buf = "application/pdf";
    } else if ( strcmp( dot, ".rtf" ) == 0 ){
        buf = "application/rtf";
	} else if ( strcmp( dot, ".doc" ) == 0  || strcmp( dot, ".dot" ) == 0 ){
        buf = "application/msword";
	} else if ( strcmp( dot, ".xls" ) == 0  || strcmp( dot, ".xla" ) == 0 ){
        buf = "application/msexcel";
	} else if ( strcmp( dot, ".hlp" ) == 0  || strcmp( dot, ".chm" ) == 0 ){
        buf = "application/mshelp";
	} else if ( strcmp( dot, ".swf" ) == 0  || strcmp( dot, ".swfl" ) == 0 || strcmp( dot, ".cab" ) == 0 ){
        buf = "application/x-shockwave-flash";
	} else if ( strcmp( dot, ".ppt" ) == 0  || strcmp( dot, ".ppz" ) == 0 || strcmp( dot, ".pps" ) == 0 || strcmp( dot, ".pot" ) == 0 ){
        buf = "application/mspowerpoint";

	/* Binary & Packages */
    } else if ( strcmp( dot, ".zip" ) == 0 ){
        buf = "application/zip";
    } else if ( strcmp( dot, ".rar" ) == 0 ){
        buf = "application/x-rar-compressed";
    } else if ( strcmp( dot, ".gz" ) == 0 ){
        buf = "application/x-gzip";
    } else if ( strcmp( dot, ".jar" ) == 0 ){
        buf = "application/java-archive";
	} else if ( strcmp( dot, ".tgz" ) == 0  || strcmp( dot, ".tar" ) == 0 ){
        buf = "application/x-tar";
	} else {
		buf = "application/octet-stream";
	}
	strcpy(ret, buf);
}
