CXX=g++
CXX_VERSION=$(shell $(CXX) -dumpversion)
STD=-std=c++14
ifeq ($(shell expr $(CXX_VERSION) '>=' 5.1), 1)
STD=-std=c++17
endif

CXXFLAGS=$(STD) -Wall -Wextra -pedantic -O3 #-DNDEBUG
LIBS=-lpcap -lpthread -lboost_system -lboost_filesystem
PROG=nfa_handler

all: $(PROG)

SRC=$(wildcard *.cpp)
HDR=$(wildcard *.h)
OBJ=$(patsubst %.cpp, %.o, $(SRC))

.PHONY: clean all

$(PROG): $(OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LIBS)

%.o: %.cpp %.h
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(LIBS)


nodata:
	rm -f obs* tmp* *.fsm *.fa *.pa *.ba

clean:
	rm -f $(PROG) *.o
