CXX=g++
CXX_VERSION=$(shell $(CXX) -dumpversion)
STD=-std=c++14
ifeq ($(shell expr $(CXX_VERSION) '>=' 5.1), 1)
STD=-std=c++17
endif

SRCDIR=src
COMMON=$(SRCDIR)/common
EXE=$(SRCDIR)/exe

CXXFLAGS=$(STD) -Wall -Wextra -pedantic  -I $(COMMON) -O3 #-Wfatal-errors #-DNDEBUG
LIBS=-lpcap -lpthread -lboost_system -lboost_filesystem

PROG=nfa_error lmin reduce
all: $(PROG)

SRC=$(wildcard $(COMMON)/*.cpp)
HDR=$(wildcard $(COMMON)/*.hpp)
OBJ=$(patsubst %.cpp, %.o, $(SRC))

.PHONY: clean all

nfa_error: $(EXE)/nfa_error.o $(OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LIBS)

$(EXE)/nfa_error.o: $(EXE)/nfa_error.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(LIBS)

reduce: $(EXE)/reduce.o $(OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LIBS)

$(EXE)/reduce.o: $(EXE)/reduce.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(LIBS)

lmin: $(EXE)/lmin.o $(OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LIBS)

$(EXE)/lmin.o: $(EXE)/lmin.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(LIBS)

matrix: $(EXE)/mc.o $(OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LIBS)

$(EXE)/mc.o: $(EXE)/mc.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(LIBS)

state_merge_mc: $(EXE)/state_merge_mc.o $(OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LIBS)

$(EXE)/state_merge_mc.o: $(EXE)/state_merge_mc.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(LIBS)

%.o: %.cpp %.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(LIBS)

no-data:
	rm -f *.fa *.dot *.jpg *.json *.jsn tmp*
	rm -f obs* tmp* *.fsm *.fa *.pa *.ba

clean:
	rm -f $(COMMON)/*.o $(EXE)/*.o $(PROG)
