#include "threadpool.h"
#include <stddef.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <assert.h>
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"
#define BUFF_SIZE 1000
#define REQ_SIZE 1500
#define CONTENT_SIZE 5000

typedef enum { FALSE = 0, TRUE } boolean;

////////////////////////////////////////////////////////////////////////////////////////////////
//..............................................................................................
////////////////////////////////////////////////////////////////////////////////////////////////

//  function prints usage error message, and exits program

void error_msg()
{
    printf("Usage: server <port> <pool-size>");
    exit(1);
}    


////////////////////////////////////////////////////////////////////////////////////////////////
//..............................................................................................
////////////////////////////////////////////////////////////////////////////////////////////////

//  function searches for first appearance of 'c' in 'string'

int get_char_index(char* string, char c)
{
    for (int i=0; i<strlen(string); i++)
    {
        if(string[i]==c)
            return i;
    }
    return -1;
}


////////////////////////////////////////////////////////////////////////////////////////////////
//..............................................................................................
////////////////////////////////////////////////////////////////////////////////////////////////

//  function gets a string and checks if its a number

boolean isNumber(char* str)
{
    for(int i=0; i<strlen(str); i++)
    {
        if(str[i]<'0' || str[i]>'9')
            return FALSE;
    }
    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////
//..............................................................................................
////////////////////////////////////////////////////////////////////////////////////////////////

//  function gets a file path and returns the file extension

char *get_mime_type(char *name)
{
    char *ext = strrchr(name, '.');

    if (!ext)
        return NULL;
     
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) 
        return "text/html";
        
    if (strcmp(ext, ".txt") == 0 ) 
        return "text/html";
        
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) 
        return "image/jpeg";
        
    if (strcmp(ext, ".gif") == 0) 
        return "image/gif";
        
    if (strcmp(ext, ".png") == 0) 
        return "image/png";
        
    if (strcmp(ext, ".css") == 0) 
        return "text/css";
        
    if (strcmp(ext, ".au") == 0) 
        return "audio/basic";
        
    if (strcmp(ext, ".wav") == 0) 
        return "audio/wav";
        
    if (strcmp(ext, ".avi") == 0) 
        return "video/x-msvideo";
        
    if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) 
        return "video/mpeg";
        
    if (strcmp(ext, ".mp3") == 0) 
        return "audio/mpeg";
        
    return NULL;
}


////////////////////////////////////////////////////////////////////////////////////////////////
//..............................................................................................
////////////////////////////////////////////////////////////////////////////////////////////////

//  function returns an HTTP error response

char* build_error_response(int code, char* path, int c_len)
{
    
    char headers[REQ_SIZE] = "HTTP/1.0 ";
    char description[REQ_SIZE];
    char* content = NULL;
    char* response = NULL;
    int content_size = c_len;
    
    // add code description to headers, and content
    sprintf(description, "%d", code);
    if(code==302)
    {
        strcat(description, " Found");
        content = "<html><head><title>302 Found</title>M/head>\n<body><h4>302 Found</h4>Directories must end with a slash.\n</body></html>";
        content_size = strlen(content);
    }
    if(code==400)
    {
        strcat(description, " Bad Request");
        content = "<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n<BODY><H4>400 Bad request</H4>\nBad Request.\n</BODY></HTML>";
        content_size = strlen(content);
    }
    if(code==403)
    {
        strcat(description, " Forbidden");
        content = "<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\n<BODY><H4>403 Forbidden</H4>\nAccess denied.\n</BODY></HTML>";
        content_size = strlen(content);
    }
    if(code==404)
    {
        strcat(description, " Not Found");
        content = "<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\n<BODY><H4>404 Not Found</H4>\nFile not found.\n</BODY></HTML>";
        content_size = strlen(content);
    }
    if(code==500)
    {
        strcat(description, " Internal Server Error");
        content = "<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\n<BODY><H4>500 Internal Server Error</H4>\nSome server side error.\n</BODY></HTML>";
        content_size = strlen(content);
    }
    if(code==501)
    {
        strcat(description, " Not supported");
        content = "<HTML><HEAD><TITLE>501 Not supported</TITLE></HEAD>\n<BODY><H4>501 Not supported</H4>\nMethod is not supported.</BODY></HTML>";
        content_size = strlen(content);
    }    
    strcat(headers, description);
    strcat(headers, "\r\nServer: webserver/1.0\r\n");
    
    //  add date to header
    time_t now;
    char time_buff[128];
    now = time(NULL);
    strftime(time_buff, sizeof(time_buff), RFC1123FMT, gmtime(&now));
    
    strcat(headers, "Date: ");
    strcat(headers, time_buff);
    strcat(headers, "\r\n");
    
    if(code==302)    
    {
        strcat(headers, "Location: ");
        strcat(headers, path);
        strcat(headers, "/\r\n");
    }

    strcat(headers, "Content-Type: text/html\r\n");
    strcat(headers, "Content-Length: ");
    
    //  add content size to header    
    char num[10];
    sprintf(num, "%d", content_size);
    strcat(headers, num);
    strcat(headers, "\r\n");

    strcat(headers, "Connection: close\r\n\r\n");
    
    //  make response (headers + content)
    response = (char*) malloc( sizeof(char)* (strlen(headers)+strlen(content)) );
    response[0] = '\0';
    strncat(response, headers, strlen(headers));
    strncat(response, content, strlen(content));

    
    return response;
 
    
}


////////////////////////////////////////////////////////////////////////////////////////////////
//..............................................................................................
////////////////////////////////////////////////////////////////////////////////////////////////

//  function makes 200 OK HTTP header

char* say_OK(char* path, int content_size)
{
    char* headers = (char*) malloc(sizeof(char)*REQ_SIZE);
    strcpy(headers, "HTTP/1.0 200 OK\r\nServer: webserver/1.0\r\n");
    
    //  add time
    
    strcat(headers, "Content-Type: ");

    if(get_mime_type(path)!=NULL)
        strcat(headers, get_mime_type(path));
    else
        strcat(headers, "text/html");
    
    strcat(headers, "\r\n");
    
    if(content_size!=0)
    {    
        strcat(headers, "Content-Length: ");
        char num[10];
        sprintf(num, "%d", content_size);
        strcat(headers, num);
        strcat(headers, "\r\n");
    }
    // strncat(headers, "Last-Modified: ");
    
    strcat(headers, "Connection: close\r\n\r\n");
    return headers;
 
}


////////////////////////////////////////////////////////////////////////////////////////////////
//..............................................................................................
////////////////////////////////////////////////////////////////////////////////////////////////

//  function writes buffer content to client
//  (if 'isText' = TRUE, 'len' parameter doesn't matter)

void respond_to_client(int* client, void* buff, boolean isText, int len)
{
    
    int bytes_to_write = 0;
    int written = 0 ;
    int sent = 0;
    
    //  check how many bytes to write
    if(isText==TRUE)
        bytes_to_write = strlen(buff);
    else
        bytes_to_write = len;
    
    //  write buffer to client
    while(TRUE)
    {
        if(bytes_to_write == 0)
            break;
            
        if(isText==TRUE)
            sent = write(*client, (char*)buff, bytes_to_write-written);
        else
            sent = write(*client, (unsigned char*)buff, bytes_to_write-written); 
        
        if(sent<0)
        {
            perror("write");
            exit(1);
        }
        
        written += sent;
        
        if(written == bytes_to_write)
            break;
    }

}


////////////////////////////////////////////////////////////////////////////////////////////////
//..............................................................................................
////////////////////////////////////////////////////////////////////////////////////////////////

//  function reads from file and immediately writes it to client

void send_file_to_client(int* client, int fd)
{
    
    unsigned char w_buff[BUFF_SIZE];
    memset(w_buff, 0, sizeof(w_buff));
    
    char* response = NULL;

    while(TRUE)
    {
        //  reset buffer
        memset(w_buff, 0, sizeof(w_buff));
        
        //  read file content to buffer
        int bytes_read = read(fd, w_buff, BUFF_SIZE);
        if (bytes_read < 0)
        {   
            response = build_error_response(500, NULL, 0);
            respond_to_client(client, response, TRUE, -1);
            close(*client);
            return;
        }
        if (bytes_read == 0)
            break;
            
        //  write buffer content to client
        respond_to_client(client, w_buff, FALSE, sizeof(w_buff));
    }
    return;
    
}


////////////////////////////////////////////////////////////////////////////////////////////////
//..............................................................................................
////////////////////////////////////////////////////////////////////////////////////////////////

//  function sends an html representation of accessed foldier content 

void send_dir_content(int* client, char *path)
{
    char* response = NULL;
    struct dirent *file;
    DIR *dir;
    
    //  open directory
    dir = opendir (path);
    if (dir == NULL) 
    {
        response = build_error_response(500, path, 0);
        respond_to_client(client, response, TRUE, -1);
        close(*client);
        return;
    }

    //  start writing html to client
    respond_to_client(client, "<HTML>\n<HEAD><TITLE>Index of ", TRUE, -1);
    respond_to_client(client, path+1, TRUE, -1);
    respond_to_client(client, "</TITLE></HEAD>\n<BODY><H4>Index of ", TRUE, -1);
    respond_to_client(client, path+1, TRUE, -1);
    respond_to_client(client, "</H4>\n<table CELLSPACING=8>\n", TRUE, -1);
    respond_to_client(client, "<tr><th>Name</th><th>Last Modified</th><th>Size</th></tr>\n", TRUE, -1);


    //  process each file in directory
    while ((file = readdir(dir)) != NULL) 
    {
        if( strcmp(file->d_name, "..")!=0  && strcmp(file->d_name, ".")!=0)
        {
            
            //  write file names to client
            respond_to_client(client, "<tr>\n<td><A HREF=\"", TRUE, -1);
            respond_to_client(client, file->d_name, TRUE, -1);
            respond_to_client(client, "\">", TRUE, -1);
            respond_to_client(client, file->d_name, TRUE, -1);
            
            
            //  get full path of file
            char* full_path = (char*) malloc(sizeof(char)* (strlen(path)+strlen(file->d_name)));
            strcpy(full_path, path);
            strcat(full_path, file->d_name);
            
            //  get stats of file
            struct stat st;
            if(stat(full_path, &st)<0)
            {
                response = build_error_response(500, path, 0);
                respond_to_client(client, response, TRUE, -1);
                close(*client);
                return;
            }
            
            //  write 'Last Modified' to client
            char time_buff[128];
            strftime(time_buff, sizeof(time_buff), RFC1123FMT, localtime(&st.st_mtime));
            respond_to_client(client, "</A></td><td>", TRUE, -1);
            respond_to_client(client, time_buff, TRUE, -1);
            respond_to_client(client, "</td>\n", TRUE, -1);
            respond_to_client(client, "</A></td><td>\n", TRUE, -1);
            
            //  write 'File size' to client
            int size = st.st_size;
            char num[50];
            sprintf(num, "%d", size);
            respond_to_client(client, num, TRUE, -1);
            respond_to_client(client, "</td>\n", TRUE, -1);
            respond_to_client(client, "</tr>\n", TRUE, -1);
            
            free(full_path);
        }
        
    }
    
    //  write the last html code
    respond_to_client(client, "</table>\n<HR>\n<ADDRESS>webserver/1.0</ADDRESS>\n</BODY></HTML>\n\n", TRUE, -1);
    
    //  close directory
    if(closedir(dir) == -1)
    {
        response = build_error_response(500, NULL, 0);
        respond_to_client(client, response, TRUE, -1);
        close(*client);
        return ;
    }
    
    return ;
}


////////////////////////////////////////////////////////////////////////////////////////////////
//..............................................................................................
////////////////////////////////////////////////////////////////////////////////////////////////

//  function parses the client's request and replies accordingly

int handle_connection(void *arg)
{
    
    int* client = (int*) arg;
    char r_buff[BUFF_SIZE];
    int read_so_far = 0;
    int nread = 0;
    char* response = NULL;
    
    //  reading client request
    while(TRUE)
    { 
        nread = read(*client, r_buff, sizeof(r_buff));
        
        if(nread<0)
        {
            response = build_error_response(500, NULL, 0);
            respond_to_client(client, response, TRUE, -1);
            free(response);
            perror("read");
            return 0;
        }
        read_so_far += nread;
        
        if(strstr(r_buff,"\n") != NULL)
            break;
        
        if(nread==0)
            break;
            
    }

    int size = get_char_index(r_buff, '\n');

    //  verify HTTP1.1 / HTTP1.0
    if( (r_buff[size-2]!='0' || r_buff[size-2]!='1')
        && r_buff[size-3]!='.' && r_buff[size-3]!='1'
        && r_buff[size-5]!='/' && r_buff[size-5]!='P'
        && r_buff[size-7]!='T' && r_buff[size-7]!='T'
        && r_buff[size-9]!='H')
    {
        response = build_error_response(400, NULL, 0);
        respond_to_client(client, response, TRUE, -1);
        free(response);
        close(*client);
        return 0;
    }

    size = size-10-4;
    
    int c_len = get_char_index(r_buff, ' ');
    char* cmd = (char*) malloc(sizeof(char)*(c_len));
    if(cmd == NULL)
    {
        response = build_error_response(500, NULL, 0);
        respond_to_client(client, response, TRUE, -1);
        free(response);
        close(*client);
        return 0;
    }
    strncpy(cmd, r_buff, c_len);
    cmd[c_len]='\0';
    
    
    //  verify 'GET' request
    if (strcmp(cmd, "GET") != 0)
    {
        response = build_error_response(501, NULL, 0);
        respond_to_client(client, response, TRUE, -1);
        free(cmd);
        free(response);
        close(*client);
        return 0;
    }
    free(cmd);
    
    //  get path
    char* path = (char*) malloc(sizeof(char)*size);  
    if(path == NULL)
    {
        response = build_error_response(500, NULL, 0);
        respond_to_client(client, response, TRUE, -1);
        free(response);
        close(*client);
        return 0;
    }
    
    strncpy(path, &r_buff[4], size);
    path[size]='\0';

    //  add . at the start of original path
    char* dot_path = (char*) malloc(sizeof(char)* (strlen(path)+1));
    if(dot_path == NULL)
    {
        response = build_error_response(500, NULL, 0);
        respond_to_client(client, response, TRUE, -1);
        close(*client);
        free(path);
        free(response);
        return 0;
    }
    strcpy(dot_path, ".");
    strncat(dot_path, path, strlen(path));
    
    struct stat st;

    //  check if path exists
    if (stat(dot_path, &st) == -1)
    {
        //  404 not found
        response = build_error_response(404, NULL, 0);
        respond_to_client(client, response, TRUE, -1);
        free(path);
        free(dot_path);
        free(response);
        close(*client);
        return 0;
    }
    char* type = get_mime_type(path);
    
    //  path exists !
    
    //  check if it's a directory
    if(type==NULL && S_ISDIR(st.st_mode))
    {
        //  check path has a '/' at the end
        if(path[strlen(path)-1]!='/')
        {
            //  302 found
            response = build_error_response(302, path, 0);
            respond_to_client(client, response, TRUE, -1);
            close(*client);
            free(path);
            free(dot_path);
            free(response);
            return 0;
        }
        
        DIR *dir; 
        struct dirent *file;
        
        //  open directory
        dir = opendir(dot_path);
        if (dir == NULL)
        {
            //  500 internal error
            response = build_error_response(500, NULL, 0);
            respond_to_client(client, response, TRUE, -1);
            free(path);
            free(dot_path);
            free(response);
            close(*client);
            return 0;
        }
        
        //  go through files in directory
        while ((file = readdir(dir)) != NULL)
        {
            //  find index.html
            if (strcmp(file->d_name, "index.html") == 0)
            {   
                //  check execution permissions
                if (S_IXOTH & st.st_mode)
                {
                    char* index_file = (char*) malloc(sizeof(char)* (strlen(dot_path)+10));
                    if(index_file == NULL)
                    {
                        response = build_error_response(500, NULL, 0);
                        respond_to_client(client, response, TRUE, -1);
                        free(path);
                        free(dot_path);
                        free(response);
                        close(*client);
                        return 0;
                    }
                    
                    strncpy(index_file, dot_path, strlen(dot_path));
                    strncat(index_file, "index.html ", 10);
                    
                    //  get index file stats
                    stat(index_file, &st);
                    
                    //  check read permissions
                    if (S_IROTH & st.st_mode)
                    {
                        //  open index file
                        int fd = open(index_file, S_IROTH, 0666);
                        if (fd == -1)
                        {
                            //  500 internal error
                            response = build_error_response(500, NULL, 0);
                            respond_to_client(client, response, TRUE, -1);
                            free(path);
                            free(dot_path);
                            free(index_file);
                            free(response);
                            close(*client);
                            return 0;
                        }
                        
                        //  send 200 OK headers
                        int file_size = st.st_size;
                        response = say_OK(path, file_size);
                        respond_to_client(client, response, TRUE, -1);
                        //  write file content to client
                        send_file_to_client(client, fd);
                        
                        //  close index file
                        if (close(fd) < 0)
                        {
                            //  500 internal
                            response = build_error_response(500, NULL, 0);
                            respond_to_client(client, response, TRUE, -1);
                            free(path);
                            free(dot_path);
                            free(index_file);
                            free(response);
                            close(*client);
                            return 0;
                        }
                    }
                    //  read permission denied
                    else
                    {
                        //  403 forbidden
                        response = build_error_response(403, NULL, 0);
                        respond_to_client(client, response, TRUE, -1);
                        free(path);
                        free(dot_path);
                        free(index_file);
                        free(response);
                        close(*client);
                        return 0;
                        
                    }
                    
                    free(path);
                    free(dot_path);
                    free(index_file);
                    free(response);
                    close(*client);
                    return 0;
                }
                //  execution permission denied
                else
                {
                    //  403 forbidden
                    response = build_error_response(403, NULL, 0);
                    respond_to_client(client, response, TRUE, -1);
                    free(path);
                    free(dot_path);
                    free(response);
                    close(*client);
                    return 0;
                }
            }
        }
        
        //  close directory
        if (closedir(dir) == -1)
        {
            //  500 internal error
            response = build_error_response(500, NULL, 0);
            respond_to_client(client, response, TRUE, -1);
            free(path);
            free(dot_path);
            free(response);
            close(*client);
            return 0;
        }
        
        //  index file not found return dir_content
        
        //  send 200 OK headers
        response = say_OK(path, 0);
        respond_to_client(client, response, TRUE, -1);
        //  write directory content to client
        send_dir_content(client, dot_path);
        
        free(path);
        free(dot_path);
        free(response);
        close(*client);
        return 0;
        
    }
    
    //  path is a file
    else
    {
        //  check read permissions
        if (S_IROTH & st.st_mode)
        {
            //  open file
            int fd = open(dot_path, S_IROTH, 0666);
            if (fd == -1)
            {
                //  500 internal
                response = build_error_response(500, NULL, 0);
                respond_to_client(client, response, TRUE, -1);
                free(path);
                free(dot_path);
                free(response);
                close(*client);
                return 0;
            }
            
            //  send 200 OK headers
            char* response = say_OK(path, 0);
            respond_to_client(client, response, TRUE, -1);
            //  send file
            send_file_to_client(client, fd);
            
            //  close file
            if (close(fd) < 0)
            {
                response = build_error_response(500, NULL, 0);
                respond_to_client(client, response, TRUE, -1);
                free(path);
                free(dot_path);
                free(response);
                close(*client);
                return 0;
            }
            
            free(path);
            free(dot_path);
            free(response);
            close(*client);
            return 0;
        }
        //  read permissions denied
        else
        {   
            //  403 forbidden
            response = build_error_response(403, NULL, 0);
            respond_to_client(client, response, TRUE, -1);
            free(path);
            free(dot_path);
            free(response);
            close(*client);
            return 0;
        }
    }

    free(path);
    free(dot_path);
    close(*client);
    return 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////
//..............................................................................................
////////////////////////////////////////////////////////////////////////////////////////////////

//  function initializes the server and manages clients requests

int main( int argc, char *argv[] )
{
    
    threadpool* pool = NULL;
    int port = 0;
    int max_requests = 0;
    
    //  input sanity check
    if(argc==4 && isNumber(argv[1])==TRUE && isNumber(argv[2])==TRUE && isNumber(argv[3])==TRUE)
    {
        port = atoi(argv[1]);
        pool = create_threadpool(atoi(argv[2]));   
        max_requests = atoi(argv[3]);
    }
    else
        error_msg();
    
    
    //  open welcome socket
    int welcome = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if ( welcome < 0 )
    {
        destroy_threadpool(pool);
        perror("socket error\n");
        exit(1);
    }
    
    //  initialize server 
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    
    //  bind welcome socket to server
    if(bind(welcome, (struct sockaddr*) &server, sizeof(server))<0)
    { 
        destroy_threadpool(pool);
        close(welcome);
        perror("bind error\n");
        exit(1);
    }
    
    //  listen for connections
    if(listen(welcome, 5)<0)
    {
        destroy_threadpool(pool);
        close(welcome);
        perror("listen error\n");
        exit(1);
    }

    int* sockets = (int*) malloc(sizeof(int)*max_requests);
    assert(sockets!=NULL);
    
    //  accept connections
    for(int i=0; i<max_requests; i++)
    {
        int new_sock = accept(welcome, NULL, NULL);
        if(new_sock<0)
        {
            perror("accept\n");
            destroy_threadpool(pool);
            close(welcome);
            free(sockets);
            exit(1);
        }
        sockets[i] = new_sock;
        
        //  dispatch thread to handle connection
        dispatch(pool, (dispatch_fn)handle_connection, (void*) &sockets[i]);
    }
    
    free(sockets);
    destroy_threadpool(pool);
    close(welcome);
    return 0;
    
}



