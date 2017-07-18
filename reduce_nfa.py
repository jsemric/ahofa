#!/usr/bin/env python3

import re
from collections import defaultdict
import os, sys
import argparse

class AutomatonException(Exception):
    def __init__(self, arg):
        self.arg = arg

    def __str__(self):
        return self.arg

class NFA:

    RE_transition_FA_format = re.compile('^\w+\s+\w+\s+\w+$')
    RE_state_FA_format = re.compile('^\w+$')
    RE_state_freq_FA_format = re.compile('^\w+\s+\d+(\s+\d+)?$')

    def __init__(self):
        self.transitions = list(defaultdict(set))
        self.initial_states = set()
        self.final_states = set()
        self.state_map = dict()
        self.state_count = 0
        self.state_freq = list()
        self.succ = list()
        self.pred = list()
        self.state_depth = list()

    def _add_state(self, state):
        if not state in self.state_map:
            self.transitions.append(defaultdict(set))
            self.state_map[state] = self.state_count
            self.state_count += 1

        return self.state_map[state]

    def _add_rule(self, pstate, qstate, symbol):
        pstate = self._add_state(pstate)
        qstate = self._add_state(qstate)
        self.transitions[pstate][int(symbol,0)].add(qstate)

    def _add_initial_state(self, state):
        state = self._add_state(state)
        self.initial_states.add(state)

    def _add_final_state(self, fstate):
        self.final_states.add(self.state_map[fstate])

    def _add_freq(self, state, freq, depth=None):
        state = self.state_map[state]
        self.state_freq[state] = int(freq)

    def _compute_pred_and_succ(self):
        self.succ = [set() for i in range(self.state_count)]
        self.pred = [set() for i in range(self.state_count)]

        for state, rules in enumerate(self.transitions):
            for key, value in rules.items():
                for q in value:
                    self.succ[state].add(q)
                    self.pred[q].add(state)

    def _compute_depth(self):
        self.state_depth = [0 for i in range(self.state_count)]
        actual = self.initial_states.copy()
        empty = set()
        depth = 0
        while True:
            empty = empty.union(actual)
            new = set()
            for q in actual:
                self.state_depth[q] = depth
                new = new.union(self.succ[q])
            new -= empty
            actual = new
            if not new:
                break
            depth += 1


    ###########################################################################
    #   IO functions                                                          #
    ###########################################################################

    def write_fa(self):
        for qs in self.initial_states:
            yield str(qs)
        for state, rules in enumerate(self.transitions):
            for key, value in rules.items():
                for q in value:
                    yield ' '.join((str(state),str(q),hex(key)))
        for qf in self.final_states:
            yield str(qf)

    def print_fa(self):
        for line in self.write_fa():
            print(line)

    def parse_fa_file(self, fname):
        rules = 0
        with open(fname, 'r') as f:
            for line in f:
                # erase new line at the end of the string
                if line[-1] == '\n':
                    line = line[:-1]

                if rules == 0:
                    # read initial state(s)
                    if NFA.RE_state_FA_format.match(line):
                        self._add_initial_state(line)
                    elif NFA.RE_transition_FA_format.match(line):
                        rules = 1
                        self._add_rule(*line.split())
                    else:
                        raise AutomatonException('invalid syntax ' + line)
                elif rules == 1:
                    # read transitions
                    if NFA.RE_transition_FA_format.match(line):
                        self._add_rule(*line.split())
                    elif NFA.RE_state_FA_format.match(line):
                        self._add_final_state(line)
                        rules = 2
                    else:
                        raise AutomatonException('invalid syntax: ' + line)
                elif rules == 2:
                    # read final states
                    if NFA.RE_state_FA_format.match(line):
                        self._add_final_state(line)
                    elif line.startswith('====='):
                        self.state_freq = [0 for x in range(self.state_count)]
                        rules = 3
                    else:
                        raise AutomatonException('invalid syntax: ' + line)
                else:
                    if NFA.RE_state_freq_FA_format.match(line):
                        self._add_freq(*line.split())
                    else:
                        raise AutomatonException('invalid syntax: ' + line)

        self._compute_pred_and_succ()
        self._compute_depth()


    ###########################################################################
    #   Reduction, etc.                                                       #
    ###########################################################################

    def process_packets(self, pcapfile):
        pass

    def add_selfloops_to_final_states(self):
        for x in self.final_states:
            for c in range(256):
                self.transitions[x][c] = set([x])

    def reduce(self, error=0, depth=None):

        total = max(self.state_freq)
        omited = 0

        # mark states with low frequency
        marked = set()
        for state, freq in enumerate(self.state_freq):
            if freq <= error:
                marked.add(state)

        # mark also state which successor has same freq
        border = [q for q in range(self.state_count) if not q in marked \
                  and self.succ[q].issubset(marked)]

        while True:
            new_bord = []
            for p in border:
                freq = 0
                for q in self.succ[p]:
                    freq += self.state_freq[p]
                if freq == self.state_freq:
                    makred.add(p)
                    new_bodr.append(p)

            if not new_bord:
                break

        for q in range(self.state_count):
            if q in marked and len(self.pred[q]) == 1:
                p, *_ = self.pred[q]
                if self.state_freq[p] == self.state_freq[q]:
                    marked.add(p)

        marked -= self.initial_states
        # prune all states with all predecessors marked
        to_prun = set([q for q in marked if marked.issuperset(self.pred[q]) \
                      and self.state_depth[q] > depth ])

        # print(to_prun)
        # rebuild NFA and compute error
        state_map = dict()
        cnt = 0
        for state in range(self.state_count):
            if not state in to_prun:
                state_map[state] = cnt
                cnt += 1
                if to_prun.issuperset(self.succ[state]):
                    for q in self.succ[state]:
                        tmp = self.state_freq[state]
                        if self.state_freq[q] != tmp:
                            omited += self.state_freq[q]

        new_transitions = [defaultdict(set) for x in range(cnt)]
        #print(new_transitions)
        for state, rules in enumerate(self.transitions):
            if state in state_map.keys():
                for key, value in rules.items():
                    tmp = value - to_prun
                    if tmp:
                        new_transitions[state_map[state]][key] = set([state_map[q] for q in tmp])

        pct = int(100.0*cnt/self.state_count)
        epct = 100.0*omited/total
        sys.stderr.write('states: ' + str(cnt) + '/' + str(self.state_count) + ' ' + \
                         str(pct) + '%\n')
        sys.stderr.write('Error: ' + str(omited) + '/' + str(total) + ' ' + str(epct) + '%\n')
        self.transitions = new_transitions
        self.final_states = set([x for x in range(cnt) if not self.transitions[x]])
        self.state_count = cnt
        self.state_freq = list()
        self._compute_pred_and_succ()
        self._compute_depth()

def main():

    parser = argparse.ArgumentParser()
    parser.add_argument('-i','--input', type=str, metavar='FILE', help='input file with automaton')
    parser.add_argument('-o','--output', type=str, metavar='FILE', help='output file, if not \
    specified everything is printed to stdout')
    parser.add_argument('-p','--pcap', type=str, metavar='FILE',help='pcap file')
    # reduction arguments
    parser.add_argument('-m','--max-freq',type=int, metavar='MAX', default=0, help='max frequency \
    of pruned state')
    parser.add_argument('-d','--depth',type=int, metavar='DEPTH', default=0, help='min depth of \
    pruned state')

    args = parser.parse_args()

    if args.input is None:
        sys.stderr.write('Error: no input specified\n')
        sys.exit(1)

    a = NFA()
    a.parse_fa_file(args.input)

    if args.pcap:
        a.process_packets(args.pcap)

    a.reduce(args.max_freq, args.depth)

    if args.output:
        with open(args.output, 'w') as out:
            for line in a.write_fa():
                out.write(line + '\n')
    else:
        a.print_fa()

if __name__ == "__main__":
    main()
