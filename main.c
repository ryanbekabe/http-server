/**
 *
MIT License

Copyright (c) 2018 Berke Emrecan ARSLAN

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>

#include "common.h"
#include "iniparse.h"

#define BACKLOG 20
#define FORWARD_INDEX 0
#define DIR_HTML_ERR "html_error/"

int sockfd;
inientry *mime_dict;

void sig_handler(int signo) {
    printf("\n");

    switch (signo) {
        case SIGTERM:
            printf("SIGTERM");
            break;
        case SIGINT:
            printf("SIGINT");
            break;
        default:
            printf("signal");
    }

    printf(" received. Shutting down.\n");

    // clear mime dictionary
    ini_destroy(mime_dict);
    // shutdown and close local socket
    shutdown(sockfd, SHUT_WR);
    close(sockfd);
    // exit the program.
    exit(0);
}

void register_signal_handlers() {
    if (signal(SIGTERM, sig_handler) == SIG_ERR) {
        error("can't catch SIGTERM");
    }

    if (signal(SIGINT, sig_handler) == SIG_ERR) {
        error("can't catch SIGINT");
    }
}

int is_directory(char *file) {
    struct stat _stat;
    stat(file, &_stat);
    return S_ISDIR(_stat.st_mode);
}

char *fext(char *file) {
    char *ext = file;

    // increment to the end
    ext += strlen(ext) - 1;

    // decrement until reach a dot
    while (*ext != '.')
        ext--;

    return ext;
}

void get_mimeinfo(char *file, char **m) {
    FILE *p;
    size_t status;
    char *ext, *mime = NULL;
    char buf[BUFFER_SIZE];

    // is custom defined ?
    ext = fext(file);
    ini_get(mime_dict, ext, &mime);

    // already defined, no need to execute "file"
    if (mime != NULL) {
        *m = mime;
        return;
    }

    bzero(buf, BUFFER_SIZE);
    strcat(buf, "file -b --mime-type --mime-encoding ");
    strcat(buf, file);
    p = popen(buf, "r");
    if (p == NULL) {
        error("can't extract mime info");
    }

    status = fread(buf, sizeof(char), BUFFER_SIZE, p);
    if (status == -1) {
        error("can't extract mime info");
    }

    fclose(p);

    *m = (char *) malloc(sizeof(char) * (strlen(buf) + 1));
    strcpy(*m, buf);
    free(mime);
}

/**
 * prepend t into s
 * @param s
 * @param t
 */
void prepend(char *s, const char *t) {
    size_t len = strlen(t);
    size_t i;

    memmove(s + len, s, strlen(s) + 1);
    for (i = 0; i < len; ++i)
        s[i] = t[i];
}

int send_all(int remotefd, char *buf, size_t *len) {
    ssize_t n = -1;
    size_t total = 0;
    size_t bytes_left = *len;

    while (total < *len) {
        n = send(remotefd, buf + total, bytes_left, 0);
        if (n == -1) {
            break;
        }

        total += n;
        bytes_left -= n;
    }

    *len = total;
    return n == -1 ? -1 : 0;
}

char *extract_requested_source(char *request) {
    int i = 0;
    char *delim = " ";
    char *token = strtok(request, delim);
    while (token != NULL) {
        if (i++ == 1) {
            break;
        }

        token = strtok(NULL, delim);
    }
    return token;
}

void filesize(FILE *f, long *size) {
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    rewind(f);
}

int has_index(char *file) {
    FILE *fp;
    char *tmp = (char *) malloc(sizeof(char) * (strlen(file) + strlen("index.html") + 1));
    strcpy(tmp, file);
    strcat(tmp, "index.html");

    fp = fopen(tmp, "r");

    free(tmp);
    if (fp == NULL) {
        return 0;
    }

    fclose(fp);
    return 1;
}

const char *get_status_str(int status_code) {
    switch (status_code) {
        default:
        case 200:
            return "OK";
        case 302:
            return "Found";
        case 403:
            return "Forbidden";
        case 404:
            return "Not Found";
    }
}

void send_http_line(int remotefd, char *str) {
    size_t len = (size_t) strlen(str);
    send_all(remotefd, str, &len);

    len = (size_t) strlen("\r\n");
    send_all(remotefd, "\r\n", &len);
}

void send_header(int remotefd, char *key, char *value) {
    char line[BUFFER_SIZE];

    bzero(line, BUFFER_SIZE);
    strcat(line, key);
    strcat(line, ": ");
    strcat(line, value);

    send_http_line(remotefd, line);
}

void send_status(int remotefd, int status) {
    char buf[BUFFER_SIZE];
    sprintf(buf, "HTTP/1.1 %d %s", status, get_status_str(status));
    send_http_line(remotefd, buf);
}

void send_file(int remotefd, char *file) {
    FILE *fp = NULL;
    ssize_t bytes = 0;
    char buf[BUFFER_SIZE];

    fp = fopen(file, "rb");
    if (fp == NULL) {
        fprintf(stderr, "%s\n", file);
        error("can't open file!!");
    }

    while ((bytes = fread(buf, sizeof(char), BUFFER_SIZE, fp)) > 0) {
        send(remotefd, buf, (size_t) bytes, 0);
    }

    fclose(fp);
}

void send_error(int remotefd, int status) {
    char buf[BUFFER_SIZE];

    sprintf(buf, "%s%d.html", DIR_HTML_ERR, status);

    send_status(remotefd, status);
    send_http_line(remotefd, "");
    send_file(remotefd, buf);
}

void handle_http_request(int remotefd) {
    int status;
    FILE *fp = NULL;
    long fsize;
    char *buf, *buf_start, *mime = NULL;
    char readbuf[BUFFER_SIZE];
    char writebuf[BUFFER_SIZE];
    ssize_t read_bytes;

    // zero-fill buffers
    bzero(readbuf, BUFFER_SIZE);

    // read request
    read_bytes = recv(remotefd, readbuf, BUFFER_SIZE, 0);
    if (read_bytes == -1) {
        error("receive error");
    }

    // copy request to another str
    buf = (char *) malloc(sizeof(char) * BUFFER_SIZE);
    buf_start = buf;
    strcpy(buf, readbuf);

    // GET /index.html HTTP/1.1 => /index.html
    buf = extract_requested_source(buf);

    // oh, REQUEST CONTAINS ".." !!!!
    if (strstr(buf, "..") != NULL) {
        send_error(remotefd, 403);
        goto clear_request;
    }

    // is requested resource exists in public folder?
    prepend(buf, "public");

    // oh, we can read file. open it!
    fp = fopen(buf, "rb");
    if (fp == NULL) {
        // oh, shit! send 404!
        send_error(remotefd, 404);
        goto clear_request;
    }

    // okay file exists, but is it a directory?
    if (is_directory(buf)) {
        if (!has_index(buf)) {
            send_error(remotefd, 403);
            goto clear_request;
        } else {
            // to forward, or not to forward :D
#if FORWARD_INDEX
            send_status(remotefd, 302);
            send_header(remotefd, "Location", "/index.html");
            goto clear_request;
#else
            fclose(fp);
            strcat(buf, "index.html");
            fp = fopen(buf, "rb");
#endif
        }
    }

    // how much we need to send?
    filesize(fp, &fsize);
    // extract mime
    get_mimeinfo(buf, &mime);

    // return status code
    send_status(remotefd, 200);
    // send mime-related headers

    // content-length
    bzero(writebuf, BUFFER_SIZE);
    sprintf(writebuf, "%ld", fsize);
    send_header(remotefd, "Content-length", writebuf);
    // content-type
    send_header(remotefd, "Content-type", mime);
    // content-encoding
    send_header(remotefd, "Content-encoding", "identity");
    send_http_line(remotefd, "");

    // we are done with mime
    free(mime);

    // send resource
    // to buffer, or not to buffer
    while ((read_bytes = fread(readbuf, sizeof(char), BUFFER_SIZE, fp)) > 0) {
        status = send_all(remotefd, readbuf, (size_t *) &read_bytes);
        if (status == -1) {
            error("send error!");
        }
    }

    clear_request:
    fclose(fp);
    free(buf_start);
}

void dump_remote_addr_info(int remotefd) {
    int status, port;
    void *_sockaddr;
    socklen_t addrlen;
    struct sockaddr addr;
    in_port_t remote_port;
    char ipstr[INET6_ADDRSTRLEN];

    // get address of remote socket
    addrlen = sizeof(addr);
    status = getpeername(remotefd, &addr, &addrlen);
    if (status == -1) {
        error("can't get peer name!");
    }

    // get remote address and port
    if (addr.sa_family == AF_INET) {
        struct sockaddr_in *_addr = (struct sockaddr_in *) &addr;
        _sockaddr = &(_addr->sin_addr);
        remote_port = _addr->sin_port;
    } else {
        struct sockaddr_in6 *_addr = (struct sockaddr_in6 *) &addr;
        _sockaddr = &(_addr->sin6_addr);
        remote_port = _addr->sin6_port;
    }

    // convert address to presentation
    if (inet_ntop(addr.sa_family, _sockaddr, ipstr, addrlen) == NULL) {
        error("inet_ntop");
    }

    // convert port to representation
    port = ntohs(remote_port);

    // write to stdout
    printf("connection from %s:%d\n", ipstr, port);
}

int main(int argc, char **argv) {
    int opt_val = 1, status, remotefd;
    socklen_t remote_addr_size;
    struct addrinfo hints, *res;
    struct sockaddr_storage remote_addr;


    if (argc < 2) {
        fprintf(stderr, "usage: bserver port\n");
        exit(1);
    }

    // load defined mime types
    mime_dict = NULL;
    ini_parse_file("mime.ini", &mime_dict);

    // register signal handlers
    register_signal_handlers();

    // load address structs
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_INET; // ipv4
    hints.ai_socktype = SOCK_STREAM; // stream please
    hints.ai_flags = AI_PASSIVE; // fill in my IP

    if ((status = getaddrinfo(NULL, argv[1], &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        exit(2);
    }

    // create and bind a socket
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1) {
        error("can't create socket");
    }

    status = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val));
    if (status == -1) {
        error("can't set SO_REUSEADDR");
    }

    status = bind(sockfd, res->ai_addr, res->ai_addrlen);
    if (status == -1) {
        error("can't bind socket");
    }

    // start listening on socket
    status = listen(sockfd, BACKLOG);
    if (status == -1) {
        error("can't listen on socket");
    }

    // dump our listen configuration
    void *addr;
    char ipstr[INET_ADDRSTRLEN];

    addr = &(((struct sockaddr_in *) res->ai_addr)->sin_addr);
    inet_ntop(res->ai_family, addr, ipstr, sizeof(ipstr));
    printf("bServer is listening on %s:%s\n", ipstr, argv[1]);

    // after this point, we are done with addrinfo
    freeaddrinfo(res);

    // prepare thingies about remote address
    remotefd = 0;
    remote_addr_size = sizeof(remote_addr);

    // accept connections
    while (remotefd != -1) {
        remotefd = accept(sockfd, (struct sockaddr *) &remote_addr, &remote_addr_size);
        if (remotefd == -1) {
            error("can't accept the connection");
        }

        // we could accept that connection.
        // dump her info
        dump_remote_addr_info(remotefd);

        // send some blah blah
        handle_http_request(remotefd);

        // we are done with this connection
        // shutdown and close
        printf("closing connection ..\n");
        shutdown(remotefd, SHUT_WR);
        close(remotefd);
    }

    // cleanup
    ini_destroy(mime_dict);
    shutdown(sockfd, SHUT_WR);
    close(sockfd);
    return 0;
}