#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <stdarg.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#define MAX_REQUEST_BODY_LENGTH (1024 * 1024)
#define LINE_BUF_SIZE 4096
#define BLOCK_BUF_SIZE (4 * 1024 * 1024)

typedef void (*sighandler_t)(int);

static void log_exit(char *fmt, ...);

static void *xmalloc(size_t sz);

struct HTTPHeaderField {
    char *name;
    char *value;
    struct HTTPHeaderField *next;
};

struct HTTPRequest {
    int protocol_minor_version;
    char *method;
    char *path;
    struct HTTPHeaderField *header;
    char *body;
    long length;
};

struct FileInfo {
    char *path;
    long size;
    int ok;
};

static void install_signal_handlers(void);

static void service(FILE *in, FILE *out, char *docroot);

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <docroot>\n", argv[0]);
        exit(1);
    }

    install_signal_handlers();
    service(stdin, stdout, argv[1]);
    exit(0);
}

static struct HTTPRequest *read_request(FILE *in);

static void respond_to(struct HTTPRequest *req, FILE *out, char *docroot);

static void free_request(struct HTTPRequest *req);

static void service(FILE *in, FILE *out, char *docroot) {
    struct HTTPRequest *req;
    req = read_request(in);
    respond_to(req, out, docroot);
    free_request(req);
}

static void read_request_line(struct HTTPRequest *req, FILE *in);

static struct HTTPHeaderField *read_header_field(FILE *in);

static long content_length(struct HTTPRequest *req);

static struct HTTPRequest *read_request(FILE *in) {
    struct HTTPRequest *req;
    struct HTTPHeaderField *h;

    req = xmalloc(sizeof(struct HTTPRequest));
    read_request_line(req, in);

    req->header = NULL;
    while ((h = read_header_field(in)) != NULL) {
        h->next = req->header;
        req->header = h;
    }

    req->length = content_length(req);
    if (req->length != 0) {
        if (req->length > MAX_REQUEST_BODY_LENGTH) {
            log_exit("request body too long");
        }
        req->body = xmalloc(req->length);
        if (fread(req->body, req->length, 1, in) < 1) {
            log_exit("failed to read request body");
        }
    } else {
        req->body = NULL;
    }
    return req;
}

static void upcase(char *str) {
    char *p;
    for (p = str; *p; p++) {
        *p = (char) toupper((int) *p);
    }
}

static void read_request_line(struct HTTPRequest *req, FILE *in) {
    char buf[LINE_BUF_SIZE];
    char *path, *p;

    // ?????????????????????GET /path/to/file HTTP/1.0\0???????????????????????????????????????
    if (!fgets(buf, LINE_BUF_SIZE, in)) {
        log_exit("no request line");
    }

    // 1?????????????????????????????????p?????????
    p = strchr(buf, ' ');
    if (!p) {
        log_exit("parse error on request line (1): %s", buf);
    }
    // ???????????????????????????????????????????????????????????????1????????????
    *p++ = '\0';

    // ????????????p??? "GET "???????????????????????????????????????????????????buf????????????????????????????????????HTTP???????????????????????????????????????????????????
    req->method = xmalloc(p - buf);
    // buf??????req->method???????????????strcpy????????????????????????????????????????????????????????????
    strcpy(req->method, buf);
    // ????????????????????????????????????
    upcase(req->method);

    // ????????????path????????????????????????????????????
    path = p;
    // 2?????????????????????????????????p?????????
    p = strchr(path, ' ');
    if (!p) {
        log_exit("parse error on request line (2): %s", buf);
    }
    // ???????????????????????????????????????????????????????????????1????????????
    *p++ = '\0';

    // ????????????path??????????????????????????????????????????????????????????????????????????????p?????????????????????????????????????????????????????????????????????
    req->path = xmalloc(p - path);
    // path??????req->path????????????
    strcpy(req->path, path);

    // HTTP??????????????????????????????HTTP1???????????????????????????
    if (strncasecmp(p, "HTTP/1.", strlen("HTTP/1.")) != 0) {
        log_exit("parse error on request line (3): %s", buf);
    }
    // ????????????p???HTTP?????????????????????????????????????????????
    p += strlen("HTTP/1.");
    // HTTP??????????????????????????????????????????
    req->protocol_minor_version = atoi(p);
}

static struct HTTPHeaderField *read_header_field(FILE *in) {
    struct HTTPHeaderField *h;
    char buf[LINE_BUF_SIZE];
    char *p;

    if (!fgets(buf, LINE_BUF_SIZE, in)) {
        log_exit("failed to read request header field: %s", strerror(errno));
    }
    if ((buf[0] == '\n') || (strcmp(buf, "\r\n") == 0)) {
        return NULL;
    }

    p = strchr(buf, ':');
    if (!p) {
        log_exit("parse error on request header field: %s", buf);
    }
    *p++ = '\0';

    h = xmalloc(sizeof(struct HTTPHeaderField));
    h->name = xmalloc(p - buf);
    strcpy(h->name, buf);

    p += strspn(p, " \t");
    h->value = xmalloc(strlen(p) + 1);
    strcpy(h->value, p);

    return h;
}

static char *lookup_header_field_value(struct HTTPRequest *req, char *name) {
    struct HTTPHeaderField *h;
    for (h = req->header; h; h = h->next) {
        if (strcasecmp(h->name, name) == 0) {
            return h->value;
        }
    }
    return NULL;
}

static long content_length(struct HTTPRequest *req) {
    char *val;
    long len;

    val = lookup_header_field_value(req, "Content-Length");
    if (!val) {
        return 0;
    }
    len = atoi(val);
    if (len < 0) {
        log_exit("negative Content-Length value");
    }
    return len;
}

static void do_file_respond(struct HTTPRequest *req, FILE *out, char *docroot);

static void method_not_allowed(struct HTTPRequest *req, FILE *out);

static void not_implemented(struct HTTPRequest *req, FILE *out);

static void not_found(struct HTTPRequest *req, FILE *out);

static void respond_to(struct HTTPRequest *req, FILE *out, char *docroot) {
    if (strcmp(req->method, "GET") == 0) {
        do_file_respond(req, out, docroot);
    } else if (strcmp(req->method, "HEAD") == 0) {
        do_file_respond(req, out, docroot);
    } else if (strcmp(req->method, "POST") == 0) {
        method_not_allowed(req, out);
    } else {
        not_implemented(req, out);
    }
}

static struct FileInfo *get_fileinfo(char *docroot, char *urlpath);

static void free_fileinfo(struct FileInfo *info);

static void output_common_header_fileds(struct HTTPRequest *req, FILE *out, char *status) {
    time_t t;
    struct tm *tm;
    char buf[LINE_BUF_SIZE];

    t = time(NULL);
    tm = gmtime(&t);
    if (!tm) {
        log_exit("gmtime failed: %s", strerror(errno));
    }

    strftime(buf, LINE_BUF_SIZE, "%a, %d %b %Y %H:%M:%S GMT", tm);
    fprintf(out, "HTTP/1.%d %s\r\n", 0, status);
    fprintf(out, "Date: %s\r\n", buf);
    fprintf(out, "Server: %s/%s\r\n", "super server", "2.3");
    fprintf(out, "Connection: close\r\n");
}

static void do_file_respond(struct HTTPRequest *req, FILE *out, char *docroot) {
    struct FileInfo *info;

    info = get_fileinfo(docroot, req->path);
    if (!info->ok) {
        free_fileinfo(info);
        not_found(req, out);
        return;
    }

    output_common_header_fileds(req, out, "200 OK");
    fprintf(out, "Content-Length: %ld\r\n", info->size);
    fprintf(out, "Content-Type: %s\r\n", "text/plain");
    //fprintf(out, "Content-Type: %s\r\n", guess_content_type(info));
    fprintf(out, "\r\n");

    if (strcmp(req->method, "HEAD") != 0) {
        int fd;
        char buf[BLOCK_BUF_SIZE];
        ssize_t n;
        size_t result;

        fd = open(info->path, O_RDONLY);
        if (fd < 0) {
            log_exit("failed to open %s: %s", info->path, strerror(errno));
        }

        for (;;) {
            n = read(fd, buf, BLOCK_BUF_SIZE);
            if (n < 0) {
                log_exit("failed to read %s: %s", info->path, strerror(errno));
            }
            if (n == 0) {
                break;
            }

            result = fwrite(buf, 1, n, out);
            if (result < n) {
                log_exit("failed to write to socket: result=%d, n=%d", result, n);
            }
        }

        close(fd);
    }
    fflush(out);
    free_fileinfo(info);
}

static void method_not_allowed(struct HTTPRequest *req, FILE *out) {
    output_common_header_fileds(req, out, "405 Method Not Allowed");
    fprintf(out, "Content-Type: %s\r\n", "text/plain");
    fprintf(out, "\r\n");
    fprintf(out, "method_not_allowed\r\n");
    fflush(out);
}

static void not_implemented(struct HTTPRequest *req, FILE *out) {
    output_common_header_fileds(req, out, "501 Not Implemented");
    fprintf(out, "Content-Type: %s\r\n", "text/plain");
    fprintf(out, "\r\n");
    fprintf(out, "not_implemented\r\n");
    fflush(out);
}

static void not_found(struct HTTPRequest *req, FILE *out) {
    output_common_header_fileds(req, out, "404 Not Found");
    fprintf(out, "Content-Type: %s\r\n", "text/plain");
    fprintf(out, "\r\n");
    fprintf(out, "not_found\r\n");
    fflush(out);
}

static char *build_fspath(char *docroot, char *urlpath) {
    char *path;
    path = xmalloc(strlen(docroot) + 1 + strlen(urlpath) + 1);
    sprintf(path, "%s%s", docroot, urlpath);
    return path;
}

static struct FileInfo *get_fileinfo(char *docroot, char *urlpath) {
    struct FileInfo *info;
    struct stat st;

    info = xmalloc(sizeof(struct FileInfo));
    info->path = build_fspath(docroot, urlpath);
    info->ok = 0;

    if (lstat(info->path, &st) < 0) {
        return info;
    }
    if (!S_ISREG(st.st_mode)) {
        return info;
    }

    info->ok = 1;
    info->size = st.st_size;
    return info;
}

static void free_fileinfo(struct FileInfo *info) {
    free(info->path);
    free(info);
}

static void free_request(struct HTTPRequest *req) {
    struct HTTPHeaderField *h, *head;
    head = req->header;
    while (head) {
        h = head;
        head = head->next;
        free(h->name);
        free(h->value);
        free(h);
    }
    free(req->method);
    free(req->path);
    free(req->body);
    free(req);
}

static void trap_signal(int sig, sighandler_t handler) {
    struct sigaction act;
    act.sa_handler = handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_RESTART;
    if (sigaction(sig, &act, NULL) < 0) {
        log_exit("sigaction() failed: %s", strerror(errno));
    }
}

static void signal_exit(int sig) {
    log_exit("exit by signal %d", sig);
}

static void install_signal_handlers(void) {
    trap_signal(SIGPIPE, signal_exit);
}

static void *xmalloc(size_t sz) {
    void *p;
    p = malloc(sz);
    if (!p) {
        log_exit("failed to allocate memory");
    }
    return p;
}

static void log_exit(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    exit(1);
}

//static int open_connect(char *host, char *service);
//
//int main(int argc, char *argv[]) {
//    int sock;
//    FILE *f;
//    char buf[1024];
//
//    sock = open_connect((argc > 1 ? argv[1] : "localhost"), "daytime");
//    f = fdopen(sock, "r");
//    if (!f) {
//        perror("fdopen(3)");
//        exit(1);
//    }
//
//    fgets(buf, sizeof buf, f);
//    fclose(f);
//    fputs(buf, stdout);
//    exit(0);
//}
//
//static int open_connect(char *host, char *service) {
//    int sock;
//    struct addrinfo hints, *res, *ai;
//
//    memset(&hints, 0, sizeof(struct addrinfo)); // hints?????????????????????
//    hints.ai_family = AF_UNSPEC;
//    hints.ai_socktype = SOCK_STREAM;
//    if ((err = getaddrinfo(host, service, &hints, &res)) != 0) {
//        fprintf(stderr, "getaddrinfo(3): %s\n", gai_strerror(err));
//        exit(1);
//    }
//
//    for (ai = res; ai; ai = ai->ai_next) {
//        sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
//        if (sock < 0) {
//            continue;
//        }
//        if (connect(sock, ai->ai_addr, ai->ai_addrlen) < 0) {
//            close(sock);
//            continue;
//        }
//        freeaddrinfo(res);
//        return sock;
//    }
//    fprintf(stderr, "socket(2)/connect(2) failed\n");
//    freeaddrinfo(res);
//    exit(1);
//}
