CXX=g++
CXXFLAGS=-std=c++11 -Wall -Wextra -pedantic -O3 -DNDEBUG
LIBS=-lpcap -lboost_program_options
PROG=nfa_handler

all: $(PROG)

SRC_FILES=$(wildcard *.cpp)
HEADERS=$(wildcard *.h)
OBJ_FILES=$(patsubst %.cpp, %.o, $(SRC_FILES))

.PHONY: clean all

$(PROG): $(OBJ_FILES)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LIBS)

nodata:
	rm -f obs* tmp* *.fsm *.fa *.pa *.ba

clean:
	rm -f $(PROG) *.o
