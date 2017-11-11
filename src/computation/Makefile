CXX=g++
CXX_VERSION=$(shell $(CXX) -dumpversion)
STD=-std=c++14
ifeq ($(shell expr $(CXX_VERSION) '>=' 5.1), 1)
STD=-std=c++17
endif

CXXFLAGS=$(STD) -Wall -Wextra -pedantic -O3 #-DNDEBUG
LIBS=-lpcap -lpthread -lboost_system -lboost_filesystem
PROG=nfa_error label_nfa

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

label_nfa.o: label_nfa.cpp pcap_reader.h
	$(CXX) $(CXXFLAGS) -c label_nfa.cpp -o $@ $(LIBS)

label_nfa: label_nfa.o nfa.o pcap_reader.h
	$(CXX) $(CXXFLAGS) label_nfa.o nfa.o -o $@ $(LIBS)

nodata:
	rm -f obs* tmp* *.fsm *.fa *.pa *.ba

clean:
	rm -f $(PROG) *.o
