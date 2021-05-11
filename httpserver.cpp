#include "httpserver.h"
#include "mime.cpp"

namespace fs = std::filesystem;

#define MAX 1024
#define SA struct sockaddr

HttpResponse::HttpResponse(int connfd)
{
    this->connfd = connfd;
}

void HttpResponse::status(int code)
{
    this->statusCode = code;
}

void HttpResponse::cors()
{
    headers["Access-Control-Allow-Origin"] = "*";
    headers["Content-Type"] = "application/json";
    headers["Access-Control-Allow-Methods"] = "GET,PUT,POST,DELETE";
    headers["Access-Control-Allow-Headers"] = "Content-Type";
}

void HttpResponse::send(std::string content)
{
    headers["Content-Length"] = std::to_string(content.length());
    sendHead();

    write(connfd, content.c_str(), content.size());
    close(connfd);
}

void HttpResponse::sendFile(std::string path)
{
    struct stat st;
    stat(path.c_str(), &st);

    int extidx = path.find_last_of('.');
    if (extidx > 0)
    {
        std::string ext = path.substr(extidx);
        // printf("ext: %s\n", ext.c_str());
        if (mime.contains(ext))
        {
            headers["Content-Type"] = mime[ext];
        }
    }

    if (FILE *source = fopen(path.c_str(), "rb"))
    {
        headers["Content-Length"] = std::to_string(st.st_size);
        sendHead();

        char buf[BUFSIZ];
        size_t size;
        while ((size = fread(buf, 1, BUFSIZ, source)))
        {
            write(connfd, buf, size);
        }

        fclose(source);
        close(connfd);
    }
    else
    {
        status(404);
        send("Not found.");
    }
}
// private
void HttpResponse::sendHead()
{
    //Connection: close
    std::string ret = statusText();
    for (const auto &[key, value] : headers)
    {
        ret += key + ": " + value + "\n";
    }
    ret += "\n";
    write(connfd, ret.c_str(), ret.size());
}

std::string HttpResponse::statusText()
{
    switch (statusCode)
    {
    case 404:
        return "HTTP/1.1 " + std::to_string(statusCode) + " Not found\n";
        break;
    case 200:
        return "HTTP/1.1 " + std::to_string(statusCode) + " OK\n";
        break;
    default:
        return "HTTP/1.1 " + std::to_string(statusCode) + " Unknown\n";
        break;
    }
}

void HttpRequest::parse(std::string buf)
{
    std::stringstream requestIn(buf);
    std::string to;
    std::getline(requestIn, to, '\n');
    //method = to.substr(0, to.find(' ', 0));

    auto iss = std::istringstream{to};
    iss >> method;
    iss >> querystring;
    int idx = querystring.find('?', 0);
    if (idx > 0)
    {
        query = querystring.substr(idx);
        url = querystring.substr(0, idx);
    }
    else
    {
        url = querystring;
    }
    iss >> proto;

    while (std::getline(requestIn, to, '\n'))
    {
        idx = to.find_first_of(":");
        if (idx > 0)
        {
            headers[to.substr(0, idx)] = ltrim_copy(to.substr(idx + 1));
        }
        else
        {
            break;
        }
    }
    // printf("method: %s\n", method.c_str());
    // printf("url: %s\n", url.c_str());
    // printf("proto: %s\n", proto.c_str());
    // printf("query: %s\n", query.c_str());
    // printf("querystring: %s\n", querystring.c_str());
    // printf("host: %s\n", headers["Host"].c_str());
    // printf("headers: %lu\n", headers.size());
}

// class HttpThread
// {
// public:
//     std::atomic<bool> shutdown_requested;

//     HttpThread()
//     {
//         shutdown_requested = false;
//     }

//     void operator()()
//     {
//         while (!shutdown_requested.load())
//         {
//             std::this_thread::sleep_for(std::chrono::seconds(1));
//         }
//         printf("Stop http server.\n");
//     }
// };

HttpServer::HttpServer(int argport)
{
    port = argport;
}

std::function<void(HttpRequest, HttpResponse)> HttpServer::Static(std::string dirin)
{

    return [dirin](HttpRequest req, HttpResponse res) {
        std::string dir = dirin;
        printf("DIR: %s\n", dir.c_str());
        auto realdir = realpath(dir.c_str(), NULL);
        if (realdir)
        {
            std::string p = req.url.substr(req.handler_path.size());
            if (!req.handler_path.ends_with("/") && !p.starts_with("/"))
            {
                res.status(404);
                res.send("Not found.");
                return;
            }
            if (!dir.ends_with("/"))
            {
                dir += "/";
            }

            auto pth = realpath((dir + p).c_str(), NULL);
            if (pth && std::string(pth).starts_with(realdir))
            {
                //res.send(pth);
                printf("Sending file: %s\n", pth);
                struct stat st;
                stat(pth, &st);

                if (S_ISREG(st.st_mode))
                {
                    res.sendFile(pth);
                }
                else
                {
                    auto pthidx = realpath((std::string(pth) + "/index.html").c_str(), NULL);
                    if (pthidx && std::string(pthidx).starts_with(realdir))
                    {
                        stat(pthidx, &st);

                        if (S_ISREG(st.st_mode))
                        {
                            res.sendFile(pthidx);
                        }
                        else
                        {
                            res.status(404);
                            res.send("Not found.");
                        }
                    }
                    else
                    {
                        std::string content = "<html><body><pre>";

                        for (const auto &entry : fs::directory_iterator(pth))
                        {
                            content += entry.path();
                            content += "\n";
                        }
                        content += "</pre></body></html>";
                        res.send(content);
                        return;
                    }
                }
            }
            else
            {
                res.status(404);
                res.send("Not found.");
                return;
            }
        }
        else
        {
            res.status(503);
            res.send("Dir not found.");
            return;
        }
    };
}

void HttpServer::use(std::string path, std::function<void(HttpRequest, HttpResponse)> func)
{
    use_handlers[path] = func;
}

void HttpServer::get(std::string path, std::function<void(HttpRequest, HttpResponse)> func)
{
    get_handlers[path] = func;
}

void HttpServer::startThread()
{
    printf("Start http server on %d port.\n", port);
    shutdown_requested = false;
    server = std::thread([&](std::atomic<bool> *is_shutdown) {
        int sockfd;
        socklen_t len;
        struct sockaddr_in servaddr, cli;

        // socket create and verification
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd == -1)
        {
            printf("socket creation failed...\n");
            exit(0);
        }
        else
            printf("Socket successfully created..\n");
        bzero(&servaddr, sizeof(servaddr));

        // assign IP, PORT
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        servaddr.sin_port = htons(port);

        // Binding newly created socket to given IP and verification
        if ((bind(sockfd, (SA *)&servaddr, sizeof(servaddr))) != 0)
        {
            printf("socket bind failed...\n");
            exit(0);
        }
        else
            printf("Socket successfully binded..\n");

        // Now server is ready to listen and verification
        if ((listen(sockfd, 5)) != 0)
        {
            printf("Listen failed...\n");
            exit(0);
        }
        else
            printf("Server listening..\n");

        len = sizeof(cli);

        while (!is_shutdown->load())
        {
            // Accept the data packet from client and verification
            int connfd = accept(sockfd, (SA *)&cli, &len);
            if (connfd < 0)
            {
                printf("server accept failed...\n");
            }
            else
            {
                handleRequest(connfd);
            }
        }
        // After chatting close the socket
        close(sockfd);
        printf("Stop http server.\n");
    },
                         &shutdown_requested);
}

void HttpServer::start()
{
    startThread();
    server.detach();
}

void HttpServer::stop()
{
    shutdown_requested = true;
}

void HttpServer::handleRequest(int argconnfd)
{
    std::thread hthread([&](int connfd) {
        char buff[MAX];
        int n;
        int err;
        bzero(buff, MAX);
        // fflush(stdout);
        err = read(connfd, buff, sizeof(buff));
        if (err == -1)
        {
            printf("err: %s\n", hstrerror(errno));
        }
        else
        {
            HttpRequest req{};
            req.parse(buff);
            HttpResponse res{connfd};

            if (get_handlers.contains(req.url))
            {
                req.handler_path = req.url;
                get_handlers[req.url](req, res);
            }
            else
            {
                for (const auto &[key, value] : use_handlers)
                {
                    if (req.url.starts_with(key))
                    {
                        req.handler_path = key;
                        value(req, res);
                        break;
                    }
                }
                res.send("empty");
            }
        }
    },
                        argconnfd);
    hthread.detach();
}