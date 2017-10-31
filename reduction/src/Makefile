CXX=g++
CXXFLAGS=-std=c++17 -Wall -Wextra -pedantic -O3 #-DNDEBUG
LIBS=-lpcap -lpthread
PROG=nfa_error

all: $(PROG)

SRC_FILES=$(wildcard *.cpp)
HEADERS=$(wildcard *.h)
OBJ_FILES=$(patsubst %.h, %.o, $(SRC_FILES))

.PHONY: clean all

nfa.o: nfa.h nfa.cpp
	$(CXX) $(CXXFLAGS) -c nfa.cpp -o $@

nfa_error.o: nfa_error.cpp pcap_reader.h
	$(CXX) $(CXXFLAGS) -c nfa_error.cpp -o $@ $(LIBS)

nfa_error: nfa_error.o nfa.o pcap_reader.h
	$(CXX) $(CXXFLAGS) nfa_error.o nfa.o -o $@ $(LIBS)

nodata:
	rm -f obs* tmp* *.fsm *.fa *.pa *.ba

clean:
	rm -f $(PROG) *.o
