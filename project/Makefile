CC=g++
CPPFLAGS=-Wall -Wextra -g
LDFLAGS= 
LDLIBS=

DEPS=transport.o

all: server client 

server: server.o $(DEPS)
client: client.o $(DEPS)

clean:
	@rm -rf server client *.bin *.o	*.out
