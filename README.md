# ahofa
ahofa - Approximate Handling of Finite Automata

## About
**ahofa** provides flexible methods for reducing non-deterministic finite automata (NFA) used in network traffic filtering. To achieve a substantial reduction of NFA, language over-approximation is used, since language preserving methods may not be sufficient. Besides reduction algorithms, other usefull programs are provided such as evaluation of reduced NFA error, and DFA minimization.

## Requirements
* g++ (at least 5.2.1)
* Python3
* numpy
* [libpcap](http://www.tcpdump.org/)
* C++ [boost](https://www.boost.org/)
### Optional
* [rabit](http://www.languageinclusion.org/doku.php?id=tools)
* [symboliclib](https://github.com/Miskaaa/symboliclib/tree/master/symboliclib)

## NFA format
The NFA format (with `.fa` extension) is line-based and has the following structure:
```
<state>  // initial state
<symbol> <state> <state>  // transitions
...
<state>  // final states
...
```

## Usage
For positional and optional arguments run with `-h` option.
Reduction and error evaluation tool.
```
./app-reduction.py
```
NFA error evaluation.
```
./nfa_eval  
```
