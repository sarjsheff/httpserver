## HTTPSERVER

Simple c++ http server. No keep-alive feature. 

### Build

```clang++ --std=gnu++2a main.cpp httpserver.cpp -o httpserver```

or

```gcc --std=gnu++2a main.cpp httpserver.cpp -lstdc++ -o httpserver```

### Usage

Register GET handler on HttpServer instanse:

```
    server.get("/test", [&](HttpRequest req, HttpResponse res) {
        res.status(200);
        res.headers["Content-Type"] = "application/json";
        res.send("{}");
    });
```

Register static file handler:

```
    server.use("/public/", server.Static("./public"));
```