## TinyS  
TinyS is a simple HTTP server, which supports both static resources and reverse proxy. 

## Usage  
```
git clone https://www.github.com/ElevenKeys/TinyS.git  
make  
sudo ./TinyS  
```

Now you can visit bilibili from localhost(127.0.0.1). O(∩_∩)O~~  

## About
TinyS is a event-driven server, whice implemented by a internal message loop. When the request comes, main thread catches it and generates a message, then the worker threads fetch it from the message queue and handle it. 

As a lock of URL routing, the HTTP server can't switch automatically between visiting local file and reverse proxy now. The function of routing may be introduced in TinyS the following version. So does the configuration.

The reverse proxy is divided into several modules. Now only direct proxy, which just transmit the request and response, is supported. The interface of fastcgi and uwsgi were given, not implemented yet.