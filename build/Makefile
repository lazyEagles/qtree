
VPATH = ../include ../src ../test \
        ../../threadpool/include ../../threadpool/src
CFLAGS = -g --std=c11

objs = body10.o qtree.o threadpool_func.o
headerdir = -I../include -I../../threadpool/include
x11flag = -L /usr/X11R6/lib -lX11 -lm

all: body10

body10: $(objs) qtree.h 
	$(CC) $(CFLAGS) $(headerdir) $^ -o $@ -pthread $(x11flag)
%.o: %.c qtree.h threadpool.h
	$(CC) $(CFLAGS) $(headerdir) -c $< -o $@ $(x11flag)

.PHONY: clean
clean:
	rm $(objs) body10
