CC=gcc
#DEBUG = -DDEBUG
DEBUG = 
CFLAGS = -Wall -O0 -g -I./ $(DEBUG)  
OUTPATH = ./bin
obj = tiny_io.o tiny_log.o tiny_poll.o tiny_socket.o  tiny_mq.o tiny_proxy_module.o tiny_worker.o tiny_start.o tiny_module.o

all:TinyS

TinyS:$(foreach v,$(obj),$(OUTPATH)/$v)
	$(CC) -lpthread -o $@ $^

$(OUTPATH):
	mkdir $(OUTPATH) 

$(OUTPATH)/tiny_io.o:tiny_io.c tiny_io.h | $(OUTPATH)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTPATH)/tiny_log.o:tiny_log.c tiny_log.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTPATH)/tiny_poll.o:tiny_poll.c tiny_poll.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTPATH)/tiny_socket.o:tiny_socket.c tiny_socket.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTPATH)/tiny_proxy_module.o:tiny_proxy_module.c 
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTPATH)/tiny_mq.o:tiny_mq.c tiny_mq.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTPATH)/tiny_worker.o:tiny_worker.c 
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTPATH)/tiny_start.o:tiny_start.c 
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTPATH)/tiny_module.o:tiny_module.c 
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf *.o $(OUTPATH)/*.o

