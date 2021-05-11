#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <atomic>
#include <thread>
#include <unistd.h>
#include <strings.h>
#include <string>
#include <map>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <filesystem>
#include <sstream>
#include "utils.h"

class HttpResponse
{
public:
    int connfd;
    std::map<std::string, std::string> headers{{"Connection", "close"}, {"Content-Type", "text/html"}};

    HttpResponse(int connfd);
    void status(int code);
    void cors();
    void sendFile(std::string path);
    void send(std::string content);

private:
    int statusCode = 200;

    void sendHead();
    std::string statusText();
};

class HttpRequest
{
public:
    std::string method;
    std::string url;
    std::string query{""};
    std::string proto;
    std::string querystring;
    std::string handler_path;
    std::map<std::string, std::string> headers{};

    void parse(std::string buf);
};

class HttpServer
{
public:
    int port;
    std::atomic<bool> shutdown_requested;
    std::thread server;
    std::map<std::string, std::function<void(HttpRequest, HttpResponse)>> get_handlers;
    std::map<std::string, std::function<void(HttpRequest, HttpResponse)>> use_handlers;

    HttpServer(int argport = 8080);
    std::function<void(HttpRequest, HttpResponse)> Static(std::string dirin);
    void use(std::string path, std::function<void(HttpRequest, HttpResponse)> func);
    void get(std::string path, std::function<void(HttpRequest, HttpResponse)> func);
    void start();
    void startThread();
    void stop();

private:
    void handleRequest(int argconnfd);
};

#endif