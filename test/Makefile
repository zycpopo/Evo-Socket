CXX ?= g++
CXXFLAGS ?= -std=c++11 -I../include -g -Wall
HEADERS := $(wildcard ../include/*.hpp ../third_party/popolog/*.hpp)

.PHONY: all check clean

all: recv_semantics_test logging_test

recv_semantics_test: recv_semantics_test.cc $(HEADERS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@ $(LDFLAGS) -pthread

logging_test: logging_test.cc logging_peer.cc $(HEADERS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) logging_test.cc logging_peer.cc -o $@ $(LDFLAGS) -pthread

check: recv_semantics_test logging_test
	./recv_semantics_test
	./logging_test .logging-test-output

clean:
	rm -f recv_semantics_test logging_test
