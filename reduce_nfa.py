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

class NetworkNFA:

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
        self.total_freq = 0

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
        yield from self.initial_states
        for state, rules in enumerate(self.transitions):
            for key, value in rules.items():
                for q in value:
                    yield ' '.join((str(state),str(q),hex(key)))
        yield from self.final_states

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
                    if NetworkNFA.RE_state_FA_format.match(line):
                        self._add_initial_state(line)
                    elif NetworkNFA.RE_transition_FA_format.match(line):
                        rules = 1
                        self._add_rule(*line.split())
                    else:
                        raise AutomatonException('invalid syntax ' + line)
                elif rules == 1:
                    # read transitions
                    if NetworkNFA.RE_transition_FA_format.match(line):
                        self._add_rule(*line.split())
                    elif NetworkNFA.RE_state_FA_format.match(line):
                        self._add_final_state(line)
                        rules = 2
                    else:
                        raise AutomatonException('invalid syntax: ' + line)
                elif rules == 2:
                    # read final states
                    if NetworkNFA.RE_state_FA_format.match(line):
                        self._add_final_state(line)
                    elif line.startswith('====='):
                        self.state_freq = [0 for x in range(self.state_count)]
                        rules = 3
                    else:
                        raise AutomatonException('invalid syntax: ' + line)
                else:
                    if NetworkNFA.RE_state_freq_FA_format.match(line):
                        self._add_freq(*line.split())
                    else:
                        raise AutomatonException('invalid syntax: ' + line)

        self._compute_pred_and_succ()
        self._compute_depth()
        self.total_freq = max(self.state_freq)
        for x in self.initial_states:
            self.state_freq[x] = self.total_freq


    ###########################################################################
    #   Reduction, etc.                                                       #
    ###########################################################################

    def add_selfloops_to_final_states(self):
        for x in self.final_states:
            for c in range(256):
                self.transitions[x][c] = set([x])

    def remove_unreachable(self):
        actual = self.initial_states
        reached = set()
        while True:
            reached = reached.union(actual)
            new = set()
            for q in actual:
                new = new.union(self.succ[q])
            new -= reached
            actual = new
            if not new:
                break

        state_map = dict()
        cnt = 0
        for x in reached:
            state_map[x] = cnt
            cnt += 1

        out = NetworkNFA()
        out.state_count = cnt
        out.transitions = [defaultdict(set) for x in range(cnt)]
        for state, rules in enumerate(self.transitions):
            if state in state_map:
                for key, val in rules.items():
                    out.transitions[state_map[state]][key] = set([state_map[x] \
                    for x in val if x in state_map])

        out.initial_states = [state_map[x] for x in self.initial_states]
        out.final_states = [state_map[x] for x in self.final_states \
                           if x in state_map]
        out._compute_pred_and_succ()
        out._compute_depth()

        return out


    def state_error(self, state):
        sf = self.state_freq[state]
        return 0 if sf == 0 else sf / self.total_freq

    def reduce(self, error=0, depth=0):
        marked = set()
        srt_states = sorted([x for x in range(self.state_count) \
                            if self.state_depth[x] >= depth], \
                            key=lambda x : self.state_freq[x])

        for state in srt_states:
            err = self.state_error(state)
            assert (err >= 0 and err <= 1)
            if err <= error:
                marked.add(state)
                error -= err
            else:
                break

        final_state_label = self.state_count
        new_transitions = [defaultdict(set) for x in range(self.state_count+1)]
        for state, rules in enumerate(self.transitions):
            for key, val in rules.items():
                new_transitions[state][key] = \
                set([x if x not in marked else final_state_label for x in val])

        out = NetworkNFA()
        out.transitions = new_transitions
        out.state_count = self.state_count + 1
        out.final_states = self.final_states.copy()
        out.initial_states = self.initial_states.copy()
        out.final_states.add(final_state_label)
        out._compute_pred_and_succ()
        out = out.remove_unreachable()
        sys.stderr.write(str(out.state_count) + '/' + str(self.state_count) + '\n')

        return out

    def merge_states(self, state1, state2):
        pass

def main():

    parser = argparse.ArgumentParser()
    parser.add_argument('-i','--input', type=str, metavar='FILE',
                        help='input file with automaton')
    parser.add_argument('-o','--output', type=str, metavar='FILE',
                        help='output file, if not specified everything is \
                        printed to stdout')
    parser.add_argument('-s','--add-sl', action='store_true',
                        help='add self-loops to final states')
    # reduction arguments
    parser.add_argument('-e','--max-error',type=float, metavar='ERR', default=0,
                        help='idk')
    parser.add_argument('-d','--depth',type=int, metavar='DEPTH', default=0,
                        help='min depth of pruned state')
    parser.add_argument('-m','--merge-rate',type=float, metavar='N', default=0,
                        help='idk')

    args = parser.parse_args()

    if args.input is None:
        sys.stderr.write('Error: no input specified\n')
        sys.exit(1)

    a = NetworkNFA()
    a.parse_fa_file(args.input)

    a = a.reduce(args.max_error, args.depth)
    if args.add_sl:
        a.add_selfloops_to_final_states()

    if args.output:
        with open(args.output, 'w') as out:
            for line in a.write_fa():
                print(line, file=out)
    else:
        a.print_fa()

if __name__ == "__main__":
    main()
