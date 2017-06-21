Usage:
git clone https://www.github.com/ElevenKeys/TinyS.git
make
sudo ./TinyS

Now you can visit bilibili from localhost(127.0.0.1).O(∩_∩)O~~

TinyS is simple HTTP server, which supports static resources and reverse proxy.It is event-driven server, whice maintains a message queue internally.When request comes, The main thread catches it and generates a message, then the worker threads fetch it from queue and handle it.

As a lock of URL routing function, the HTTP server can't switch between static file and reverse proxy automatically now.The routing may be introduced in TinyS the following version.So does the configuration.

The proxy is divides into modules.Now only direct proxy, which just transmit the raw request and response, is supported,the interface of fastcgi and uwsgi were given, yet not implemented.