all:
	c++ -g -pthread -std=c++11 src/myhttpd.cpp -o myhttpd
clean:
	rm -f *.out myhttpd
