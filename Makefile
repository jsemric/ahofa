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

PROG=nfa_eval lnfa state_frequency
all: $(PROG)

SRC=$(wildcard $(COMMON)/*.cpp)
HDR=$(wildcard $(COMMON)/*.hpp)
OBJ=$(patsubst %.cpp, %.o, $(SRC))

.PHONY: clean all

nfa_eval: $(EXE)/nfa_eval.o $(OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LIBS)

$(EXE)/nfa_eval.o: $(EXE)/nfa_eval.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(LIBS)

state_frequency: $(EXE)/state_frequency.o $(OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LIBS)

$(EXE)/state_frequency.o: $(EXE)/state_frequency.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(LIBS)

# merging inspired by predicate logic
state_merge_mc: $(EXE)/state_merge_mc.o $(OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LIBS)

$(EXE)/state_merge_mc.o: $(EXE)/state_merge_mc.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(LIBS)

%.o: %.cpp %.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(LIBS)

no-data:
	rm -f *.fa *.dot *.jpg *.json *.jsn tmp*
	rm -f obs* tmp* *.fsm *.fa *.pa *.ba *csv

pack:
	rm -f xsemri00.zip
	zip -r xsemri00.zip src/*/*.hpp src/*/*.cpp Makefile nfa.py \
	get_nfa_size.py draw_nfa.py app-reduction.py reduction.py \
	reduction_eval.py README.txt examples

clean:
	rm -f $(COMMON)/*.o $(EXE)/*.o $(PROG)
