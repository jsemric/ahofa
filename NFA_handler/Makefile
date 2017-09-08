CXX=g++
CXXFLAGS=-std=c++11 -Wall -Wextra -pedantic -O3 #-DNDEBUG
LIBS=-lpcap -lboost_program_options
PROG=nfa_handler

all: $(PROG)

SRC_FILES=$(wildcard *.cpp)
HEADERS=$(wildcard *.h)
OBJ_FILES=$(patsubst %.h, %.o, $(SRC_FILES))

.PHONY: clean all

nfa.o: nfa.h nfa.cpp
	$(CXX) $(CXXFLAGS) -c nfa.cpp -o $@

pcap_reader.o: pcap_reader.h pcap_reader.cpp
	$(CXX) $(CXXFLAGS) -c pcap_reader.cpp -o $@ -lpcap

nfa_handler.o: nfa_handler.cpp
	$(CXX) $(CXXFLAGS) -c $^ -o $@ $(LIBS)

nfa_handler: nfa_handler.o nfa.o pcap_reader.o
	$(CXX) $(CXXFLAGS) nfa_handler.o nfa.o pcap_reader.o -o $@ $(LIBS)

nodata:
	rm -f obs* tmp* *.fsm *.fa *.pa *.ba

clean:
	rm -f $(PROG) *.o
