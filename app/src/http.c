#include "http.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <signal.h>

#include "array.h"
#include "string.h"

#define CONNMAX 1000

static int listenfd, clients[CONNMAX];
static void error(char *);
static void startServer(const char *);
static void respond(int);

typedef struct header_t {
    char * name;
    char * value;
} header_t;

typedef struct header2_t {
    string_t name;
    string_t value;
} header2_t;
typedef array_t(header2_t) header_array_t;
static header_array_t request_headers = array_new(header2_t);

static header_t reqhdr[17] = { {"\0", "\0"} };
static int clientfd;

char *request_header(const char* name);

void route(string_t method, string_t uri){
    if (string_equals(uri, string_literal("/index")) && string_equals(method, string_literal("GET"))){
        printf("HTTP/1.1 200 OK\r\n\r\n");
        printf("Hello! You are using %s", request_header("User-Agent"));
        return;
    }
    
    printf("HTTP/1.1 500 Not Handled\r\n\r\n The server has no handler to the request.\r\n");
}

void serve_forever(const char *PORT){
    struct sockaddr_in clientaddr;
    socklen_t addrlen;
    char c;    
    
    int slot=0;
    
    printf(
            "Server started %shttp://127.0.0.1:%s%s\n",
            "\033[92m",PORT,"\033[0m"
            );

    // Setting all elements to -1: signifies there is no client connected
    int i;
    for (i=0; i<CONNMAX; i++)
        clients[i]=-1;
    startServer(PORT);
    
    // Ignore SIGCHLD to avoid zombie threads
    signal(SIGCHLD,SIG_IGN);

    // ACCEPT connections
    while (1)
    {
        addrlen = sizeof(clientaddr);
        clients[slot] = accept (listenfd, (struct sockaddr *) &clientaddr, &addrlen);

        if (clients[slot]<0)
        {
            perror("accept() error");
        }
        else
        {
            if ( fork()==0 )
            {
                respond(slot);
                exit(0);
            }
        }

        while (clients[slot]!=-1) slot = (slot+1)%CONNMAX;
    }
}

//start server
void startServer(const char *port)
{
    struct addrinfo hints, *res, *p;

    // getaddrinfo for host
    memset (&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if (getaddrinfo( NULL, port, &hints, &res) != 0)
    {
        perror ("getaddrinfo() error");
        exit(1);
    }
    // socket and bind
    for (p = res; p!=NULL; p=p->ai_next)
    {
        int option = 1;
        listenfd = socket (p->ai_family, p->ai_socktype, 0);
        setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
        if (listenfd == -1) continue;
        if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0) break;
    }
    if (p==NULL)
    {
        perror ("socket() or bind()");
        exit(1);
    }

    freeaddrinfo(res);

    // listen for incoming connections
    if ( listen (listenfd, 1000000) != 0 )
    {
        perror("listen() error");
        exit(1);
    }
}


// get request header
char *request_header(const char* name)
{
    header_t *h = reqhdr;
    while(h->name) {
        if (strcmp(h->name, name) == 0) return h->value;
        h++;
    }
    return NULL;
}

//client connection
void respond(int n)
{
    int rcvd, fd, bytes_read;
    char *ptr;

    int buffer_size = 65535;
    string_t buffer;
    char * x = malloc(buffer_size);
    rcvd=recv(clients[n], x, buffer_size, 0);
    buffer.chars = x;

    if (rcvd<0)    // receive error
        fprintf(stderr,("recv() error\n"));
    else if (rcvd==0)    // receive socket closed
        fprintf(stderr,"Client disconnected upexpectedly.\n");
    else    // message received
    {
        buffer.size = rcvd;
        char   
            *qs;        // "a=1&b=2"     things after  '?'

        char *payload;     // for POST
        int payload_size;

        string_t header_string;

        // see RFC 2616, Section 5.1
        // https://www.rfc-editor.org/rfc/rfc2616#section-5.1
        string_t request_line_string;
        string_split(buffer, '\n', &request_line_string, &header_string);
        request_line_string = string_strip(request_line_string);

        string_t method;
        string_split(request_line_string, ' ', &method, &request_line_string);

        string_t uri;
        string_split(request_line_string, ' ', &uri, &request_line_string);

        string_t protocol;
        string_split(request_line_string, ' ', &protocol, &request_line_string);


        // string_split(buffer, string_literal("\n"), &method, &substring);
        // string_split(substring, string_literal(" \t"), &uri, &substring);
        // string_split(substring, string_literal(" \t\r\n"), &protocol, &substring);
        // method = strtok(buf,  " \t\r\n");
        // uri    = strtok(NULL, " \t");
        // prot   = strtok(NULL, " \t\r\n"); 

        // fprintf(stderr, "\x1b[32m + [%s] %s\x1b[0m\n", method, uri);
        
        // if (qs = strchr(uri, '?'))
        // {
        //     *qs++ = '\0'; //split URI
        // } else {
        //     qs = uri - 1; //use an empty string
        // }

        // header_t *h = reqhdr;
        // char *t, *t2;
        // while(h < reqhdr+16) {
        //     char *k,*v,*t;
        //     k = strtok(NULL, "\r\n: \t"); if (!k) break;
        //     v = strtok(NULL, "\r\n");     while(*v && *v==' ') v++;
        //     h->name  = k;
        //     h->value = v;
        //     h++;
        //     fprintf(stderr, "[H] %s: %s\n", k, v);
        //     t = v + 1 + strlen(v);
        //     if (t[1] == '\r' && t[2] == '\n') break;
        // }
        // t++; // now the *t shall be the beginning of user payload
        // t2 = request_header("Content-Length"); // and the related header if there is  
        // payload = t;
        // payload_size = t2 ? atol(t2) : (rcvd-(t-buf));

        // bind clientfd to stdout, making it easier to write
        clientfd = clients[n];
        dup2(clientfd, STDOUT_FILENO);
        close(clientfd);

        // call router
        route(method, uri);

        // tidy up
        fflush(stdout);
        shutdown(STDOUT_FILENO, SHUT_WR);
        close(STDOUT_FILENO);
    }

    //Closing SOCKET
    shutdown(clientfd, SHUT_RDWR);         //All further send and recieve operations are DISABLED...
    close(clientfd);
    clients[n]=-1;
}