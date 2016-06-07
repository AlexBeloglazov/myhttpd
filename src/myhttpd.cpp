#include <iostream>
#include <sstream>      // stringstream
#include <vector>
#include <unistd.h>     // gethostname() gethostbyname()
#include <time.h>       // time functions
// #include <cstring>      // needed for memset
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

void print_usage(const char * exec) {
    std::cout   << "\nUSAGE: " << exec << " [Options]\n\n"
                << "Options:\n" << "\t-d\t\tEnter debugging mode;\n"
                << "\t-h\t\tPrint a usage summary;\n"
                << "\t-l <file>\tLog all requests to the given file;\n"
                << "\t-p <port>\tListen on the given port;\n"
                << "\t-r <dir>\tSet root directory for the server;\n"
                << "\t-t <time>\tSet queuing time in seconds;\n"
                << "\t-n <threads>\tSet number of threads. Default: 4;\n"
                << "\t-s <policy>\tSet scheduling policy: FCFS or SJF. Default: FCFS;\n\n";
    exit(0);
}

/* Helper method to print errno to stderr */
void pr_error(const char * msg) {
    perror(msg);
    exit(1);
}

/*
 * Helper method to convert UNIX timestamp to required time format for logging.
 * With no argument given returns current date and time.
 */
const std::string get_time(time_t stamp=time(0)) {
    char t[50];
    strftime(t, sizeof(t), "%d/%b/%Y:%X %z", localtime(&stamp));
    return t;
}

/* Helper method to extract IP address as a string from sockaddr_in structure */
const std::string get_ip(struct sockaddr_in * ci) {
    char str_ip[INET_ADDRSTRLEN];
    inet_ntop(ci->sin_family, &(ci->sin_addr), str_ip, sizeof str_ip);
    return str_ip;
}

/*
 * Helper method gets a list of all files (hidden ignored) in a given directory
 * and returns it in HTML format. If directory doesn't exist returns NULL
 */
std::string get_dir_listing(const char * dir_name) {
    std::stringstream listing;
    int dir;
    struct dirent **item;
    if ((dir = scandir(dir_name, &item, NULL, alphasort)) != 0) {
        listing << HTMLOPEN << "<h2>Listing of " << dir_name << ":</h2><br>\n";
        for(int i=0; i<dir; i++) {
            if (item[i]->d_name[0] != '.') {
                // printf ("%s\n", item[i]->d_name);
                listing << item[i]->d_name << "<br>\n";
            }
            free(item[i]);
        }
        free(item);
        listing << HTMLCLOSE;
        return listing.str();
    }
    return NULL;
}

/*
 * Helper method substitutes ~ with current user's home directory path + /myhttpd
 * If requested path doesn't start with ~ then appends server's root directory to it
 */
void normalize_path(std::string * path) {
    if ((*path)[0] == '~') {
        path->erase(0, 1);
        if ((*path)[0] != '/') path->insert(0, "/");
        path->insert(0, "/myhttpd");
        path->insert(0, getpwuid(getuid())->pw_dir);
    }
    else {
        path->insert(0, serv_params.root_dir);
    }
}

/* Helper method to parse command line arguments */
void parse_args(int ac, char * av[]) {
    const char * exec_name = av[0];
    try {
        for(int i=1; i<ac; i++) {
            std::string current = av[i];
            if (current[0] == '-') {
                switch (current[1]) {
                    case 'd':
                    serv_params.debugging = true;
                    serv_params.threads = 1;
                    break;
                    case 'h':
                    print_usage(exec_name);
                    case 'l':
                    if (++i >= ac) print_usage(exec_name);
                    serv_params.log_file = av[i];
                    break;
                    case 'p':
                    if (++i >= ac) print_usage(exec_name);
                    serv_params.port = av[i];
                    break;
                    case 'r':
                    if (++i >= ac) print_usage(exec_name);
                    serv_params.root_dir = av[i];
                    break;
                    case 't':
                    if (++i >= ac) print_usage(exec_name);
                    serv_params.q_time = std::stoi(av[i]);
                    break;
                    case 'n':
                    if (++i >= ac) print_usage(exec_name);
                    serv_params.threads = std::stoi(av[i]);
                    break;
                    case 's':
                    {
                        if (++i >= ac) print_usage(exec_name);
                        std::string policy = av[i];
                        if (policy == "FCFS" || policy == "SJF") {
                            serv_params.fcfs_policy = (policy == "FCFS") ? true : false;
                        }
                        else print_usage(exec_name);
                        break;
                    }
                }
            }
            else print_usage(exec_name);
        }
    }
    catch (std::invalid_argument e) {
        print_usage(exec_name);
    }
}

int main(int argc, char * argv[]) {
    int socket_fd, y = 1;
    struct addrinfo socket_init_info, *socket_info;
    parse_args(argc, argv);

    /* Filling up socket_init_info with initial information */
    memset(&socket_init_info, 0, sizeof(socket_init_info));                 // Emptying the structure
    socket_init_info.ai_family = AF_INET;                                   // Use IPv4 address
    socket_init_info.ai_flags = AI_PASSIVE;                                 // Get the address of localhost
    socket_init_info.ai_socktype = SOCK_STREAM;                             // Socket type set to TCP

    /* Filling up socket_info structure */
    getaddrinfo(NULL, serv_params.port.c_str(), &socket_init_info, &socket_info);
    /* Creating a socket */
    if ((socket_fd = socket(socket_info->ai_family, socket_info->ai_socktype, socket_info->ai_protocol)) == -1)
        pr_error("error while creating socket");
    /* Set REUSEPORT option so no waiting time for the kernel to release that port */
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &y, socket_info->ai_addrlen) == -1)
        pr_error("failed setting socket option");
    /* Binding socket to the port */
    if ((bind(socket_fd, socket_info->ai_addr, socket_info->ai_addrlen)) == -1)
        pr_error("cannot associate socket with given port");
    /* Openning the port on localhost. SOMAXCONN defines queue length of completely established sockets */
    if ((listen(socket_fd, SOMAXCONN)) == -1)
        pr_error("cannot open the port");

    if (serv_params.debugging) {
        /* Debugging mode */
        char buffer[REQUESTSIZE];
        struct sockaddr_in con_info;
        socklen_t con_socklen = sizeof(con_info);
        int con_fd;
        std::cout   << "\n* Debugging mode.\n* Waiting for incoming connections at "
                    << get_ip((sockaddr_in *)socket_info->ai_addr) << ":" << serv_params.port << "\n\n";

        while (true) {
            memset(buffer, 0, sizeof(buffer));
            con_fd = accept(socket_fd, (struct sockaddr *) &con_info, &con_socklen);
            std::cout << get_ip(&con_info) << " connected at " << get_time() << std::endl;
            const char * msg = "\nWelcome to myhttpd v.0.0.1\n\n";
            send(con_fd, msg, strlen(msg), 0);
            recv(con_fd, buffer, sizeof(buffer), 0);
            // strtok(buffer, "\n\t");
            std::cout << buffer << std::endl;
            close(con_fd);
        }

        close(socket_fd);
    }
    else {
        /* Daemonized mode */
    }
    freeaddrinfo(socket_info);
    return EXIT_SUCCESS;
}
