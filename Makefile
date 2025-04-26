all: server subscriber

server: server.cpp
	g++ -o server server.cpp

subscriber: client.cpp
	g++ -o subscriber client.cpp

clean:
	rm -f server subscriber