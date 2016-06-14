
#include "myhttpd.h"


int socket_fd, con_fd, y = 1;
struct addrinfo socket_init_info, *socket_info;
struct sockaddr_in con_info;
socklen_t con_socklen = sizeof(con_info);
Log logging;
std::queue<http_request *> request_queue;


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
                    try {
                        logging.setlogfile(av[i]);
                        serv_params.logfile = true;
                    }
                    catch (...) {
                        std::cout << "error: cannot open log file" << std::endl;
                    }
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

/* Helper method to print errno to stderr */
void pr_error(const char * msg) {
    perror(msg);
    exit(1);
}

void create_socket_open_port() {
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
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &y, socket_info->ai_addrlen) == -1)
        pr_error("failed setting socket option");
    /* Binding socket to the port */
    if ((bind(socket_fd, socket_info->ai_addr, socket_info->ai_addrlen)) == -1)
        pr_error("cannot associate socket with given port");
    /* Openning the port on localhost. SOMAXCONN defines queue length of completely established sockets */
    if ((listen(socket_fd, SOMAXCONN)) == -1)
        pr_error("cannot open the port");
}

/* Helper method prints debugging message and queuing counter to standart output */
void print_debugging_message() {
    std::cout   << "\n* Debugging mode.\n* Waiting for incoming connections at "
                << get_ip((sockaddr_in *)socket_info->ai_addr) << ":" << serv_params.port << "\n\n";
    int i = 0;
    while (i < serv_params.q_time) {
        std::cout << "* Queuing time: " << i << " s" << '\r' << std::flush;
        sleep(1);
        i++;
    }
    std::cout << "* Queuing time: " << i << " s" << "\n\n";
}

/*
 * Helper method to convert UNIX timestamp to required time format for logging.
 * With no argument given returns current date and time.
 */
const std::string get_time_for_logging(time_t stamp=time(0)) {
    char t[50];
    strftime(t, sizeof(t), "%d/%b/%Y:%X %z", localtime(&stamp));
    return t;
}

 /* Helper method to convert UNIX timestamp to GMT time representation */
const std::string get_time_in_gmt(time_t stamp=time(0)) {
    char t[50];
    strftime(t, sizeof(t), "%a, %d %h %Y %T GMT", gmtime(&stamp));
    return t;
}

/* Helper method to extract IP address as a string from sockaddr_in structure */
const std::string get_ip(struct sockaddr_in * ci) {
    char str_ip[INET_ADDRSTRLEN];
    inet_ntop(ci->sin_family, &(ci->sin_addr), str_ip, sizeof str_ip);
    return str_ip;
}

/* Helper method returns request type as integer */
int get_method_as_int(const char * method) {
    if (strcmp(method, HTTP_REQUEST_GET_S) == 0)
        return HTTP_REQUEST_GET;
    else if (strcmp(method, HTTP_REQUEST_HEAD_S) == 0)
        return HTTP_REQUEST_HEAD;
    else return -1;
}

/* Helper method returns request status as string */
const char * get_status_as_string(int code) {
    switch (code) {
        case HTTP_STATUS_CODE_OK:
            return HTTP_STATUS_CODE_OK_S;
        case HTTP_STATUS_CODE_NOTFOUND:
            return HTTP_STATUS_CODE_NOTFOUND_S;
    }
    return HTTP_STATUS_CODE_BAD_REQUEST_S;
}

/*
 * Helper method substitutes ~ with current user's home directory path + /myhttpd
 * If requested path doesn't start with ~ then appends server's root directory to it
 */
std::string normalize_path(char const * page) {
    std::string normalized(page);
    if (normalized.front() == '~') {
        normalized.erase(0, 1);
        normalized.insert(0, "/myhttpd");
        normalized.insert(0, getpwuid(getuid())->pw_dir);
    }
    else if (!strlen(page) || page[0] != '/')
        return "";
    else normalized.insert(0, serv_params.root_dir);
    return normalized;
}

/* Helper method returns size of SERVER_INDEX_FILE or requested directory */
off_t get_filesize(std::string * norm_path) {
    struct stat f_info;
    f_info.st_size = 0;
    /* Try to get stat for requested file */
    if (!stat(norm_path->c_str(), &f_info)) {
        off_t dir_size = f_info.st_size;
        /* Check whether file is directory */
        if (S_ISDIR(f_info.st_mode)) {
            if (norm_path->back() != '/') norm_path->append("/");
            if (!stat((*norm_path + SERVER_INDEX_FILE).c_str(), &f_info)) {
                norm_path->append(SERVER_INDEX_FILE);
                return f_info.st_size;
            }
            return dir_size;
        }
        else return f_info.st_size;
    }
    return 0;
}

/* Helper method returns extension of a file provided through path argument */
extension get_file_extension(const char * path) {
    if (strcasestr(path, "html") || strcasestr(path, "htm"))
        return HTML;
    if (strcasestr(path, "jpg") || strcasestr(path, "jpeg"))
        return JPEG;
    return UNKNOWN;
}

void build_response_header(http_response & resp) {
    std::stringstream header;
    header << SERVER_HTTP_PROTOCOL_VERSION << " " << get_status_as_string(resp.req_status) << '\n';
    header << "Date: " << get_time_in_gmt() << '\n';
    header << "Server: " << SERVER_INFO << '\n';
    if (resp.mod_time)
        header << "Last-Modified: " << get_time_in_gmt(resp.mod_time) << '\n';
    if (!resp.content_type.empty())
        header << "Content-Type: " << resp.content_type << '\n';
    if (resp.content_length)
        header << "Content-Length: " << resp.content_length << '\n';
    header << '\n';
    resp.header = header.str();
}

/*
 * Helper method fills up resp.content with content of requested file or with
 * content of directory in HTML format.
 */
void get_file_content(http_request *req, http_response &resp) {
    struct stat f_info;
    std::ifstream in_f;
    /* Check if path is an empty string */
    if (req->norm_path.empty()) {
        resp.req_status = HTTP_STATUS_CODE_BAD_REQUEST;
        return;
    }
    /* Get stat for a file */
    stat(req->norm_path.c_str(), &f_info);
    /* If openned file is a directory then get list of files*/
    if (S_ISDIR(f_info.st_mode)) {
        if (get_method_as_int(req->method) == HTTP_REQUEST_GET) {
            std::stringstream listing;
            int dir;
            struct dirent **item;
            dir = scandir(req->norm_path.c_str(), &item, NULL, alphasort);
            listing << "<html>\n<head><title>Directory Listing</title></head>\n<body>\n"
                    << "<h2>Listing of " << req->page << ":</h2><br>\n";
            for(int i=0; i<dir; i++) {
                if (item[i]->d_name[0] != '.') {
                    listing << item[i]->d_name << "<br>\n";
                }
                free(item[i]);
            }
            free(item);
            listing << "</body>\n</html>\n";
            resp.content_length = listing.str().length();
            resp.content = new char[resp.content_length];
            strcpy(resp.content, listing.str().c_str());
            resp.content_type = TYPE_MIME_TEXT_HTML;
        }
        resp.req_status = HTTP_STATUS_CODE_OK;
        resp.mod_time = f_info.st_mtimespec.tv_sec;
    }
    /* Its a file */
    else if (S_ISREG(f_info.st_mode)){
        switch(get_file_extension(req->norm_path.c_str())){
            case HTML:
                in_f.open(req->norm_path);
                if (!in_f.is_open()) {
                    // cannot open
                }
                if (get_method_as_int(req->method) == HTTP_REQUEST_GET) {
                    resp.content = new char[f_info.st_size];
                    in_f.read(resp.content, f_info.st_size);
                    resp.content_length = f_info.st_size;
                }
                resp.content_type = TYPE_MIME_TEXT_HTML;
                resp.req_status = HTTP_STATUS_CODE_OK;
                resp.mod_time = f_info.st_mtimespec.tv_sec;
                break;
            case JPEG:
                in_f.open(req->norm_path, std::ios::binary);
                if (!in_f.is_open()) {
                    // cannot open
                }
                if (get_method_as_int(req->method) == HTTP_REQUEST_GET) {
                    resp.content = new char[f_info.st_size];
                    in_f.read(resp.content, f_info.st_size);
                    resp.content_length = f_info.st_size;
                }
                resp.content_type = TYPE_MIME_IMAGE_JPEG;
                resp.req_status = HTTP_STATUS_CODE_OK;
                resp.mod_time = f_info.st_mtimespec.tv_sec;
                break;
            default:
                resp.req_status = HTTP_STATUS_CODE_BAD_REQUEST;
        in_f.close();
        }
    }
    else resp.req_status = HTTP_STATUS_CODE_NOTFOUND;
}

std::string get_logstring(http_request * req, http_response & resp) {
        std::stringstream out;
        out     << req->rem_ip << " ~ [" << get_time_for_logging(req->timestamp) << "] ["
                << get_time_for_logging() << "] \"" << req->method << " " << req->page
                << " " << req->http << "\" " << resp.req_status << " "
                << resp.content_length << std::endl;
        return out.str();
}

void Log::setlogfile(char * path) {
    this->_logfile.open(path, std::ios::app);
}

void Log::doit(std::string log_str) {
    if (serv_params.debugging) {
        std::lock_guard<std::mutex> lg(this->m);
        std::cout << log_str;
    }
    else if (serv_params.logfile) {
        std::lock_guard<std::mutex> lg(this->m);
        this->_logfile << log_str << std::flush;
    }
}

void scheduling_thread() {
    if (serv_params.debugging) print_debugging_message();
    else sleep(serv_params.q_time);

    while (true) {
        if (!request_queue.empty()) {

            // This will be in a worker thread
            // ---------
            struct http_response resp;
            struct http_request * req = request_queue.front();
            request_queue.pop();
            switch (get_method_as_int(req->method)) {
                case HTTP_REQUEST_GET:
                case HTTP_REQUEST_HEAD:
                get_file_content(req, resp);
                break;
                default:
                resp.req_status = HTTP_STATUS_CODE_BAD_REQUEST;
            }
            build_response_header(resp);
            logging.doit(get_logstring(req, resp));
            send(con_fd, resp.header.c_str(), strlen(resp.header.c_str()), 0);
            if (resp.content_length) {
                send(con_fd, resp.content, resp.content_length, 0);
                delete [] resp.content;
            }
            close(req->con_fd);
            delete req;
            // ----------

        }
        sleep(1);
    }
}

void * worker_thread(void * data) {
    // TODO
    return NULL;
}

/* Queuing thread */
int main(int argc, char * argv[]) {
    parse_args(argc, argv);
    create_socket_open_port();
    /* Creating scheduling thread */
    std::thread scheduler(scheduling_thread);
    /* Accepting and queuing requests */
    while (true) {
        char    header[HEAD_LINE_LENGTH] = {'\0'}, method[METHOD_LENGTH] = {'\0'},
                page[PAGE_LENGTH] = {'\0'}, http[HTTP_LENGTH] = {'\0'};

        con_fd = accept(socket_fd, (struct sockaddr *) &con_info, &con_socklen);
        recv(con_fd, header, HEAD_LINE_LENGTH, 0);
        sscanf(header, "%5s %1024s %8s", method, page, http);
        /* Creating object that represents client request*/
        struct http_request * request = new http_request();
        request->con_fd = con_fd;
        request->norm_path = normalize_path(page);
        request->f_size = get_filesize(&request->norm_path);
        request->timestamp = time(0);
        strcpy(request->method, method);
        strcpy(request->page, page);
        strcpy(request->http, http);
        strcpy(request->rem_ip, get_ip(&con_info).c_str());
        /* Put request object into the main queue */
        request_queue.push(request);
    }

    scheduler.join();

    /* Cleaning up */
    close(socket_fd);
    freeaddrinfo(socket_info);

    return EXIT_SUCCESS;
}
