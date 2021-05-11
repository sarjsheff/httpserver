#include <stdio.h>
#include <stdlib.h>
#include <ctime>
#include <chrono>
#include <thread>
#include "httpserver.h"

int main(int argc, char *argv[])
{
    HttpServer server{8081};

    server.get("/test", [&](HttpRequest req, HttpResponse res) {
        res.send("testfunc");
    });

    //server.use("/public/", server.Static("./public"));

    server.startThread();
    server.server.join();
}