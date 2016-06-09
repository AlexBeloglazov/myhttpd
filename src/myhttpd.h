#include <iostream>
#include <sstream>      // stringstream
#include <vector>
#include <queue>
#include <unistd.h>     // gethostname() gethostbyname()
#include <time.h>       // time functions
#include <cstring>      // needed for memset
#include <sys/socket.h> // needed for the socket functions
#include <netdb.h>      // contains structures
#include <arpa/inet.h>  // inet functions
#include <dirent.h>     // dirscan function
#include <pwd.h>        // needed to get a path of user's homedirectory


#define REQUESTSIZE     1024
#define HTMLOPEN        "<html>\n<head><title>Directory Listing</title></head>\n<body>\n"
#define HTMLCLOSE       "</body>\n</html>\n"



/* Structure holds default parameters of the server */
static struct parameters {
    bool debugging = false;
    std::string port = "8080";
    std::string log_file = "";
    std::string root_dir = ".";
    int q_time = 60;
    int threads = 4;
    bool fcfs_policy = true;    // SJF if false
} serv_params;


struct worker_data {

};


struct connection {
    int s_fd;
    sockaddr addr;
    socklen_t addrlen;
};

struct http_request_data {

};

struct http_response_data {

};
