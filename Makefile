CXX=g++
CXX_VERSION=$(shell $(CXX) -dumpversion)
STD=-std=c++14
ifeq ($(shell expr $(CXX_VERSION) '>=' 5.1), 1)
STD=-std=c++17
endif

SRCDIR=src
COMMON=$(SRCDIR)/common
PDIR=$(SRCDIR)/prog

CXXFLAGS=$(STD) -Wall -Wextra -pedantic -I $(COMMON) -O3 #-DNDEBUG
LIBS=-lpcap -lpthread -lboost_system -lboost_filesystem
PROG=nfa_handler

all: nfa_handler matrix state_merge_mc lmin

SRC=$(wildcard $(COMMON)/*.cpp)
HDR=$(wildcard $(COMMON)/*.hpp)
OBJ=$(patsubst %.cpp, %.o, $(SRC))

.PHONY: clean all

nfa_handler: $(PDIR)/nfa_handler.o $(OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LIBS)

$(PDIR)/nfa_handler.o: $(PDIR)/nfa_handler.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(LIBS)

lmin: $(PDIR)/lmin.o $(OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LIBS)

$(PDIR)/lmin.o: $(PDIR)/lmin.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(LIBS)

matrix: $(PDIR)/mc.o $(OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LIBS)

$(PDIR)/mc.o: $(PDIR)/mc.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(LIBS)

state_merge_mc: $(PDIR)/state_merge_mc.o $(OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LIBS)

$(PDIR)/state_merge_mc.o: $(PDIR)/state_merge_mc.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(LIBS)

%.o: %.cpp %.h
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(LIBS)

no-data:
	rm -f *.fa *.dot *.jpg *.json *.jsn tmp*
	rm -f obs* tmp* *.fsm *.fa *.pa *.ba

clean:
	rm -f $(COMMON)/*.o $(PDIR)/*.o nfa_handler
