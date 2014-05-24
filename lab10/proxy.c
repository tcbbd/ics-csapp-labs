/*
 * proxy.c - CS:APP Web proxy
 *
 * TEAM MEMBERS:
 *     丁卓成, tcbbd@sjtu.edu.cn
 *
 * IMPORTANT: Give a high level description of your code here. You
 * must also provide a header comment at the beginning of each
 * function that describes what that function does.
 */

#include "csapp.h"

/*
 * Function prototypes
 */
int parse_uri(char *uri, char *target_addr, char *path, int  *port);
void parse_uri2(char *uri, char *target_addr, int  *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);
void proxy(int connfd, struct sockaddr_in *clientaddr, FILE *log, int id);
void *thread(void *vargp);
#define SMALLBUF 32

/* Global variables */

sem_t mutex_open_clientfd;
sem_t mutex_log;
sem_t mutex_race;

void unix_error_w(const char *msg) /* unix-style error */
{
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
}

void dns_error_w(const char *msg) /* dns-style error */
{
    fprintf(stderr, "%s: DNS error %d\n", msg, h_errno);
}

/*
 * open_clientfd - open connection to server at <hostname, port>
 *   and return a socket descriptor ready for reading and writing.
 *   Returns -1 and sets errno on Unix error.
 *   Returns -2 and sets h_errno on DNS (gethostbyname) error.
 */
/* $begin open_clientfd */
int open_clientfd_ts(char *hostname, int port)
{
    int clientfd;
    struct hostent *hp;
    struct sockaddr_in serveraddr;

    if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	return -1; /* check errno for cause of error */

    /* Fill in the server's IP address and port */
    P(&mutex_open_clientfd);
    hp = gethostbyname(hostname);
    V(&mutex_open_clientfd);
    if (hp == NULL)
	return -2; /* check h_errno for cause of error */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)hp->h_addr_list[0],
	  (char *)&serveraddr.sin_addr.s_addr, hp->h_length);
    serveraddr.sin_port = htons(port);

    /* Establish a connection with the server */
    if (connect(clientfd, (SA *) &serveraddr, sizeof(serveraddr)) < 0)
	return -1;
    return clientfd;
}
/* $end open_clientfd */

int Open_clientfd_ts(char *hostname, int port)
{
    int rc;

    if ((rc = open_clientfd_ts(hostname, port)) < 0) {
	if (rc == -1)
	    unix_error_w("Open_clientfd Unix error");
	else
	    dns_error_w("Open_clientfd DNS error");
    }
    return rc;
}

/**********************************
 * Wrappers for robust I/O routines
 **********************************/
ssize_t Rio_readn_w(int fd, void *ptr, size_t nbytes)
{
    ssize_t n;

    if ((n = rio_readn(fd, ptr, nbytes)) < 0)
	unix_error_w("Rio_readn error");
    return n;
}

int Rio_writen_w(int fd, void *usrbuf, size_t n)
{
    if (rio_writen(fd, usrbuf, n) != n) {
	    unix_error_w("Rio_writen error");
        return -1;
    }
    return n;
}

ssize_t Rio_readnb_w(rio_t *rp, void *usrbuf, size_t n)
{
    ssize_t rc;

    if ((rc = rio_readnb(rp, usrbuf, n)) < 0)
	unix_error_w("Rio_readnb error");
    return rc;
}

ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen)
{
    ssize_t rc;

    if ((rc = rio_readlineb(rp, usrbuf, maxlen)) < 0)
	unix_error_w("Rio_readlineb error");
    return rc;
}

struct targ {
    int connfd;
    struct sockaddr_in clientaddr;
    FILE *log;
    int id;
};



/*
 * main - Main routine for the proxy program
 */
int main(int argc, char **argv)
{
    int port, listenfd;
    socklen_t clientlen;
    struct targ arg;
    pthread_t tid;

    /* Check arguments */
    if (argc != 2) {
	    fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
	    exit(0);
    }

    /* Get port number */
    errno = 0;
    port = strtol(argv[1], NULL, 10);
    if (errno != 0 || port < 0 || port > 65535) {
        fprintf(stderr, "Invalid port number!\n");
        exit(0);
    }

    arg.log = fopen("proxy.log", "a");
    Sem_init(&mutex_open_clientfd, 0, 1);
    Sem_init(&mutex_log, 0, 1);
    Sem_init(&mutex_race, 0, 0);
    Signal(SIGPIPE, SIG_IGN);

    listenfd = Open_listenfd(port);
    int i = 0;
    while (1) {
        clientlen = sizeof(arg.clientaddr);
        arg.connfd = Accept(listenfd, (SA *)&arg.clientaddr, &clientlen);
        arg.id = i;
        Pthread_create(&tid, NULL, thread, &arg);
        P(&mutex_race);
        i++;
    }
    fclose(arg.log);
    exit(0);
}

void *thread(void *vargp) {
    struct targ * arg = (struct targ *)vargp;
    int connfd = arg->connfd;
    struct sockaddr_in clientaddr = arg->clientaddr;
    FILE *log = arg->log;
    int id = arg->id;
    V(&mutex_race);
    Pthread_detach(Pthread_self());

    /* For Debugging */
    //fprintf(stdout, "thread%d start connfd: %d\n", id, connfd);
    //fflush(stdout);

    proxy(connfd, &clientaddr, log, id);

    /* For Debugging */
    //fprintf(stdout, "thread%d end connfd: %d\n", id, connfd);
    //fflush(stdout);

    Close(connfd);

    /* For Debugging */
    //fprintf(stdout, "One Hit!\n");
    //fflush(stdout);
    return NULL;
}

/* return 0 if failed, return 1 if succeeded */
int deal_TE_body(rio_t *rio_read, int writefd, char *buf, int *count) {
    /* Chunked-Body   = *chunk
     *                  last-chunk
     *                  trailer
     *                  CRLF
     *
     * chunk          = chunk-size [ chunk-extension ] CRLF
     *                  chunk-data CRLF
     * chunk-size     = 1*HEX
     * last-chunk     = 1*("0") [ chunk-extension ] CRLF
     *
     * chunk-extension= *( ";" chunk-ext-name [ "=" chunk-ext-val ] )
     * chunk-ext-name = token
     * chunk-ext-val  = token | quoted-string
     * chunk-data     = chunk-size(OCTET)
     * trailer        = *(entity-header CRLF) */
    int n;
    int chunk_number = 0;
    while (1) {
        if ((n = Rio_readlineb_w(rio_read, buf, MAXLINE)) == 0) /* EOF */
            return 0;
        *count += n;

        char *pos;
        errno = 0;
        int chunk_size = strtol(buf, &pos, 16);
        if (errno != 0 || buf == pos || !(isspace(*pos) || *pos == ';'
                    || *pos == '\r') || (chunk_size == 0 && chunk_number == 0))
            return 0;
        if (Rio_writen_w(writefd, buf, n) == -1 ) return 0;

        if (chunk_size == 0) {
            int trail_complete = 0;
            while ((n = Rio_readlineb_w(rio_read, buf, MAXLINE)) != 0) {
                if (Rio_writen_w(writefd, buf, n) == -1) return 0;
                *count += n;
                if (strcmp("\r\n", buf) == 0) {
                    trail_complete = 1;
                    break;
                }
            }
            if (!trail_complete) return 0;
            else return 1;
        }

        int chunk_count = chunk_size + 2;
        while (chunk_count != 0) {
            if (chunk_count > MAXLINE)
                n = Rio_readnb_w(rio_read, buf, MAXLINE);
            else
                n = Rio_readnb_w(rio_read, buf, chunk_count);
            *count += n;
            if (n == 0)
                return 0;
            if (Rio_writen_w(writefd, buf, n) == -1) return 0;
            chunk_count -= n;
        }
        chunk_number++;
    }

    return 0;
}


/* return 0 if failed, return 1 if succeeded */
int deal_message(rio_t *rio_read, int writefd, char* buf, int *count, int no_message_body) {
    /* generic-message = start-line
     *                   *(message-header CRLF)
     *                   CRLF
     *                   [ message-body ] */
    int n;

    /* *(message-header CRLF) */
    int head_complete = 0;
    int transfer_encoding = 0;
    int content_length = -1;
    int content_count = 0;
    char field_name[SMALLBUF];
    while ((n = Rio_readlineb_w(rio_read, buf, MAXLINE)) != 0) {
        if (Rio_writen_w(writefd, buf, n) == -1) return 0;
        *count += n;

        sscanf(buf, "%s %*s", field_name);
        if (strcmp("Transfer-Encoding:", field_name) == 0)
            transfer_encoding = 1;
        if (strcmp("Content-Length:", field_name) == 0) {
            if (content_count != 0) return 0;
            char content_str[SMALLBUF];
            sscanf(buf, "%*s %s", content_str);
            char *content_pos;
            errno = 0;
            content_length = strtol(content_str, &content_pos, 10);
            if (errno != 0 || content_str == content_pos)
                return 0;
            content_count++;
        }

        if (strcmp(buf, "\r\n") == 0) /* End of Head */ {
            head_complete = 1;
            break;
        }
    }
    if (!head_complete) return 0;

    /* Response: [ message-body ] */
    if(no_message_body) return 1;

    if (transfer_encoding)
        return deal_TE_body(rio_read, writefd, buf, count);

    if (content_length >= 0) {
        int body_count = content_length;
        while (body_count != 0) {
            if (body_count > MAXLINE)
                n = Rio_readnb_w(rio_read, buf, MAXLINE);
            else
                n = Rio_readnb_w(rio_read, buf, body_count);
            *count += n;
            if (n == 0) return 0;
            if (Rio_writen_w(writefd, buf, n) == -1) return 0;
            body_count -= n;
        }
    }
    return 1;
}

void proxy(int connfd, struct sockaddr_in *clientaddr, FILE *log, int id) {
    int n, count = 0;
    char buf[MAXLINE], uri[MAXLINE], method[SMALLBUF];
    rio_t rio_conn;

    /* Request: Request-Line = Method SP Request-URI SP HTTP-Version CRLF */
    Rio_readinitb(&rio_conn, connfd);
    if ((n = Rio_readlineb_w(&rio_conn, buf, MAXLINE)) == 0) /* EOF */
        return;
    sscanf(buf, "%s %s %*s", method, uri);

    /* For Debugging */
    //fprintf(stdout, "thread%d Request-Line: %s\n", id, buf);
    //fflush(stdout);

    int port, clientfd;
    char hostname[MAXLINE], pathname[MAXLINE];
    rio_t rio_client;

    /* parse_uri can't process CONNECT method */
    if (strcmp("CONNECT", method) == 0)
        parse_uri2(uri, hostname, &port);
    else
        parse_uri(uri, hostname, pathname, &port);

    /* For Debugging */
    //fprintf(stdout, "thread%d hostname: %s port: %d\n", id, hostname, port);
    //fflush(stdout);

    clientfd = Open_clientfd_ts(hostname, port);
    if (clientfd < 0)
        return;
    Rio_readinitb(&rio_client, clientfd);

    /* CONNECT method */
    if (strcmp("CONNECT", method) == 0) {
        int head_complete = 0;
        while ((n = Rio_readlineb_w(&rio_conn, buf, MAXLINE)) != 0) {
            if (strcmp(buf, "\r\n") == 0) {
                head_complete = 1;
                break;
            }
        }
        if (!head_complete) {
            Close(clientfd);
            return;
        }
        strcpy(buf, "HTTP/1.1 200 Connection Established\r\n\r\n");
        if (Rio_writen_w(connfd, buf, strlen(buf)) == -1) {
            Close(clientfd);
            return;
        }

        fd_set read_set, ready_set;
        FD_ZERO(&read_set);
        FD_SET(connfd, &read_set);
        FD_SET(clientfd, &read_set);
        int nfds = connfd > clientfd ? connfd + 1 : clientfd + 1;
        while (1) {
            ready_set = read_set;
            Select(nfds, &ready_set, NULL, NULL, NULL);
            if (FD_ISSET(connfd, &ready_set)) {
                n = Rio_readn_w(connfd, buf, 1);
                if (Rio_writen_w(clientfd, buf, n) == -1) {
                    Close(clientfd);
                    return;
                }
            }
            if (FD_ISSET(clientfd, &ready_set)) {
                n = Rio_readn_w(clientfd, buf, 1);
                if (Rio_writen_w(connfd, buf, n) == -1) {
                    Close(clientfd);
                    return;
                }
            }
        }
    }

    if (Rio_writen_w(clientfd, buf, n) == -1) {
        Close(clientfd);
        return;
    }

    if (!deal_message(&rio_conn, clientfd, buf, &count, 0)) {
        Close(clientfd);
        return;
    }

    /* Response: Status-Line = HTTP-Version SP Status-Code SP Reason-Phrase CRLF */
    if ((n = Rio_readlineb_w(&rio_client, buf, MAXLINE)) == 0) /* EOF */ {
        Close(clientfd);
        return;
    }
    count = n;

    /* All 1xx (informational), 204 (no content), and 304 (not modified) responses
     * MUST NOT include a message-body.*/
    char status[SMALLBUF];
    int no_message_body = 0;
    sscanf(buf, "%*s %s %*s", status);
    if (status[0] == '1' || !strcmp(status, "204") || !strcmp(status, "304"))
        no_message_body = 1;
    if (Rio_writen_w(connfd, buf, n) == -1) {
        Close(clientfd);
        return;
    }

    char logentry[MAXLINE];
    int success = deal_message(&rio_client, connfd, buf, &count, no_message_body);
    format_log_entry(logentry, clientaddr, uri, count);
    P(&mutex_log);
    if (success)
        fprintf(log, "%s\n", logentry);
    else
        fprintf(log, "%s (FAILED!)\n", logentry);
    fflush(log);
    V(&mutex_log);
    Close(clientfd);

    /* For Debugging */
    //fprintf(stdout, "thread%d close clientfd: %d\n", id, clientfd);
    return;
}

void parse_uri2(char *uri, char *hostname, int *port) {
    char *hostend;
    int len;

    hostend = strpbrk(uri, ":");
    len = hostend - uri;
    strncpy(hostname, uri, len);
    hostname[len] = '\0';

    *port = atoi(hostend+1);
}


/*
 * parse_uri - URI parser
 *
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, char *pathname, int *port)
{
    char *hostbegin;
    char *hostend;
    char *pathbegin;
    int len;

    if (strncasecmp(uri, "http://", 7) != 0) {
	hostname[0] = '\0';
	return -1;
    }

    /* Extract the host name */
    hostbegin = uri + 7;
    hostend = strpbrk(hostbegin, " :/\r\n\0");
    len = hostend - hostbegin;
    strncpy(hostname, hostbegin, len);
    hostname[len] = '\0';

    /* Extract the port number */
    *port = 80; /* default */
    if (*hostend == ':')
	*port = atoi(hostend + 1);

    /* Extract the path */
    pathbegin = strchr(hostbegin, '/');
    if (pathbegin == NULL) {
	pathname[0] = '\0';
    }
    else {
	pathbegin++;
	strcpy(pathname, pathbegin);
    }

    return 0;
}

/*
 * format_log_entry - Create a formatted log entry in logstring.
 *
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), and the size in bytes
 * of the response from the server (size).
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr,
		      char *uri, int size)
{
    time_t now;
    char time_str[MAXLINE];
    unsigned long host;
    unsigned char a, b, c, d;

    /* Get a formatted time string */
    now = time(NULL);
    strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

    /*
     * Convert the IP address in network byte order to dotted decimal
     * form. Note that we could have used inet_ntoa, but chose not to
     * because inet_ntoa is a Class 3 thread unsafe function that
     * returns a pointer to a static variable (Ch 13, CS:APP).
     */
    host = ntohl(sockaddr->sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;


    /* Return the formatted log entry string */
    sprintf(logstring, "%s: %d.%d.%d.%d %s %d", time_str, a, b, c, d, uri, size);
}


