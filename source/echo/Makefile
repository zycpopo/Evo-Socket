CXX ?= g++
CXXFLAGS ?= -std=c++11 -O2 -g -Wall
HEADERS := $(wildcard ../../include/*.hpp ../../third_party/popolog/*.hpp) echo.hpp
.PHONY: all clean
all: main
main: main.cc $(HEADERS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -I../../include $< -o $@ $(LDFLAGS) -pthread
clean:
	rm -f main
