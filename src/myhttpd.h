#ifndef MYHTTPD_H
#define MYHTTPD_H

#include <condition_variable>
#include <functional>
#include <iostream>
#include <thread>
#include <sstream>      // string stream
#include <fstream>      // file stream
#include <vector>
#include <queue>
#include <mutex>
#include <sys/stat.h>   // stat systemcall
#include <unistd.h>     // gethostname() gethostbyname()
#include <time.h>       // time functions
#include <cstring>      // needed for memset
#include <sys/socket.h> // needed for the socket functions
#include <netdb.h>      // contains structures
#include <arpa/inet.h>  // inet functions
#include <dirent.h>     // dirscan function
#include <pwd.h>        // needed to get a path of user's homedirectory

/* Server settings */
#define SERVER_INFO                         "myhttpd/0.0.1"
#define SERVER_HTTP_PROTOCOL_VERSION        "HTTP/1.0"
#define SERVER_DEFAULT_PORT                 "8080"
#define SERVER_DEFAULT_ROOT_DIR             ""
#define SERVER_DEFAULT_Q_TIME               60
#define SERVER_DEFAULT_N_THREADS            4
#define SERVER_DEFAULT_FCFS                 true
#define SERVER_DEFAULT_DEBUGGING            false
#define SERVER_INDEX_FILE                   "index.html"

/* Limits for 1st line */
#define HEAD_LINE_LENGTH                    1040
#define METHOD_LENGTH                       6       // 5 + EOL
#define PAGE_LENGTH                         1025    // 1024 + EOL
#define HTTP_LENGTH                         9      // 8 + EOL

/* Accepting methods as string */
#define HTTP_REQUEST_GET_S                  "GET"
#define HTTP_REQUEST_HEAD_S                 "HEAD"

/* Accepting methods as integers */
#define HTTP_REQUEST_GET                    0
#define HTTP_REQUEST_HEAD                   1

/* Status codes as integers */
#define HTTP_STATUS_CODE_OK                 200
#define HTTP_STATUS_CODE_BAD_REQUEST        400
#define HTTP_STATUS_CODE_NOTFOUND           404

/* Status codes as strings */
#define HTTP_STATUS_CODE_OK_S               "200 OK"
#define HTTP_STATUS_CODE_BAD_REQUEST_S      "400 Bad Request"
#define HTTP_STATUS_CODE_NOTFOUND_S         "404 Not Found"

/* Supported MIME types */
#define TYPE_MIME_IMAGE_JPEG                "image/jpeg"
#define TYPE_MIME_TEXT_HTML                 "text/html"

/* Structure holds default parameters of the server */
static struct parameters {
    bool debugging = SERVER_DEFAULT_DEBUGGING;
    std::string port = SERVER_DEFAULT_PORT, logfile;
    std::string root_dir = SERVER_DEFAULT_ROOT_DIR;
    int q_time = SERVER_DEFAULT_Q_TIME;
    int threads = SERVER_DEFAULT_N_THREADS;
    bool fcfs_policy = SERVER_DEFAULT_FCFS;
} serv_params;

struct http_request {
    int con_fd;
    off_t f_size;
    char page[PAGE_LENGTH], method[METHOD_LENGTH], http[HTTP_LENGTH];
    std::string norm_path;
    time_t timestamp;
    char rem_ip[INET_ADDRSTRLEN];
};

struct http_response {
    unsigned int content_length = 0;
    std::string header, content_type;
    char * content = NULL;
    time_t mod_time = 0;
    int req_status;
};

/* Class for thread-safe logging */
class Log {
public:
    void execute(std::string);
    void openlogfile(std::string);
private:
    std::mutex m;
    std::ofstream _logfile;
};

enum extension {
    HTML,
    JPEG,
    UNKNOWN
};


void daemon_mode();
void print_usage(const char *);
void pr_error(const char *);
void parse_args(int, char *);
void create_socket_open_port();
const std::string get_time_for_logging(time_t);
const std::string get_time_in_gmt(time_t);
const std::string get_ip(struct sockaddr_in *);
void print_debugging_message();
int get_method_as_int(const char *);
const char * get_status_as_string(int);
std::string normalize_path(char const *);
void build_response_header(http_response &);
void scheduling_thread();
void worker_thread(int);
off_t get_filesize(std::string *);
extension get_file_extension(const char *);
void get_file_content(http_request *, http_response &);
bool compare_time(http_request *, http_request *);
bool compare_size(http_request *, http_request *);


#endif
