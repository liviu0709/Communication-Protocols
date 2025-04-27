all: server subscriber

server: server.cpp help.cpp
	g++ -o server server.cpp help.cpp

subscriber: client.cpp
	g++ -o subscriber client.cpp

match: test.cpp help.cpp
	g++ -o match help.cpp test.cpp -lgtest -pthread -lgtest_main
	# ./match

demo: match
	./match --gtest_filter=MatchFunctionTest.demo
clean:
	rm -f server subscriber