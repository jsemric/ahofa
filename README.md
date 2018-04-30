# ahofa
ahofa - Approximate Handling of Finite Automata

## About
**ahofa** provides flexible methods for reducing non-deterministic finite automata (NFA) used in network traffic filtering. To achieve a substantial reduction of NFA, language over-approximation is used, since language preserving methods may not be sufficient. Besides reduction algorithms, other usefull programs are provided such as evaluation of reduced NFA error, and automata minimization.

## Requirements
* g++ (at least 5.2.1)
* Python3  
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
### Main Programs
Reduction and packet frequency computation tool.
```
./reduce
```
NFA error evaluation.
```
./nfa_eval  
```
Lightweight NFA minimization.
```
./lmin
```
### Other
Rabit and Reduce tool wrapper.
```
./rabit.py
```
Automata to DOT converter.
```
./draw_nfa.py
```
DFA minimization.
```
./dfa_min.py
```
### Examples
Pruning reduction to 20% of original size.
```
./reduce examples/original-nfa/web-php.rules.fa examples/pcaps/darpa-1998-training-week1-friday.pcap.part
 -r 0.2
```
Merging reduction to 20% of original size.
```
./reduce examples/original-nfa/backdoor.rules.fa examples/pcaps/darpa-1998-training-week1-friday.pcap.part
 -r 0.2 -i 1
```
Computing packet frequency.
```
./reduce examples/original-nfa/backdoor.rules.fa examples/pcaps/darpa-1998-training-week1-friday.pcap.part
 -f
```
Reduced NFA evaluation.
```
./reduce examples/original-nfa/backdoor.rules.fa backdoor.rules-reduced.fa examples/pcaps/* -n 2
```
