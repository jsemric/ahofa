#!/usr/bin/env python3

import re
from collections import defaultdict
import os, sys
import argparse

class lazyproperty:

    def __init__(self, func):
        self.func = func

    def __get__(self, instance, cls):
        if instance is None:
            return self
        else:
            value = self.func(instance)
            setattr(instance, self.func.__name__, value)
            return value

class NetworkNFA:

    RE_transition_FA_format = re.compile('^\w+\s+\w+\s+\w+$')
    RE_state_FA_format = re.compile('^\w+$')
    RE_state_freq_FA_format = re.compile('^\w+\s+\d+(\s+\d+)?$')

    def __init__(self):
        self._transitions = list(defaultdict(set))
        self._initial_state = None
        self._final_states = set()
        self._state_map = dict()
        self._state_freq = list()
        self._state_depth = list()

    @lazyproperty
    def total_freq(self):
        return max(self._state_freq)

    @property
    def state_count(self):
        return len(self._transitions)

    @property
    def pred(self):
        pred = [set() for i in range(self.state_count)]

        for state, rules in enumerate(self._transitions):
            for key, value in rules.items():
                for q in value:
                    pred[q].add(state)

        return pred

    @property
    def succ(self):
        succ = [set() for i in range(self.state_count)]

        for state, rules in enumerate(self._transitions):
            for key, value in rules.items():
                for q in value:
                    succ[state].add(q)

        return succ

    def _add_state(self, state):
        if not state in self._state_map:
            self._state_map[state] = self.state_count
            self._transitions.append(defaultdict(set))

        return self._state_map[state]

    def _add_rule(self, pstate, qstate, symbol, usemap=True):
        if usemap:
            pstate = self._add_state(pstate)
            qstate = self._add_state(qstate)
            symbol = int(symbol,0)
        self._transitions[pstate][symbol].add(qstate)

    def _add_initial_state(self, state):
        state = self._add_state(state)
        self._initial_state = state

    def _add_final_state(self, fstate):
        self._final_states.add(self._state_map[fstate])

    def _add_freq(self, state, freq, depth=None):
        state = self._state_map[state]
        self._state_freq[state] = int(freq)

    def _compute_depth(self):
        succ = self.succ
        self._state_depth = [0 for i in range(self.state_count)]
        actual = set([self._initial_state])
        empty = set()
        depth = 0
        while True:
            empty = empty.union(actual)
            new = set()
            for q in actual:
                self._state_depth[q] = depth
                new = new.union(succ[q])
            new -= empty
            actual = new
            if not new:
                break
            depth += 1


    ###########################################################################
    #   IO functions                                                          #
    ###########################################################################

    def write_fa(self):
        yield self._initial_state
        for state, rules in enumerate(self._transitions):
            for key, value in rules.items():
                for q in value:
                    yield ' '.join((str(state),str(q),hex(key)))
        yield from self._final_states

    def print_fa(self):
        for line in self.write_fa():
            print(line)

    @classmethod
    def parse_fa_file(cls, fname):
        res = cls()
        rules = 0
        with open(fname, 'r') as f:
            for line in f:
                # erase new line at the end of the string
                if line[-1] == '\n':
                    line = line[:-1]

                if rules == 0:
                    # read initial state
                    if cls.RE_state_FA_format.match(line):
                        res._add_initial_state(line)
                        rules = 1
                    else:
                        raise RuntimeError('invalid syntax: \"' + line + '\"')
                elif rules == 1:
                    # read transitions
                    if cls.RE_transition_FA_format.match(line):
                        res._add_rule(*line.split())
                    elif cls.RE_state_FA_format.match(line):
                        res._add_final_state(line)
                        rules = 2
                    else:
                        raise RuntimeError('invalid syntax: \"' + line + '\"')
                elif rules == 2:
                    # read final states
                    if cls.RE_state_FA_format.match(line):
                        res._add_final_state(line)
                    elif line.startswith('====='):
                        res._state_freq = [0 for x in range(res.state_count)]
                        rules = 3
                    else:
                        raise RuntimeError('invalid syntax: \"' + line + '\"')
                else:
                    if cls.RE_state_freq_FA_format.match(line):
                        res._add_freq(*line.split())
                    else:
                        raise RuntimeError('invalid syntax: \"' + line + '\"')

        res._compute_depth()

        return res


    ###########################################################################
    #   Reduction, etc.                                                       #
    ###########################################################################

    def _has_path_over_alph(self, state1, state2):
        alph = [1 for x in range(256)]
        for key,val in self._transitions[state1].items():
            if state2 in val:
                alph[key] = 0
            else:
                return False

        return not sum(alph)

    def remove_same_states(self):
        '''
        TODO Add comment.
        '''
        tmp = set()
        succ = self.succ
        pred = self.pred
        for s in self.succ[self._initial_state]:
            if self._has_path_over_alph(self._initial_state,s) and \
               self._has_path_over_alph(s,s):
                tmp.add(s)

        state = tmp.pop()
        # remove states & add transitions
        for s in tmp:
            for p in pred[s]:
                for key,val in self._transitions[p].items():
                    val.discard(s)
            for key,val in self._transitions[s].items():
                for x in val:
                    self._add_rule(state,x,key,False)

        self = self.remove_unreachable()

    def add_selfloop(self, state):
        for c in range(256):
            self._transitions[state][c] = set([state])

    def add_selfloops_to_final_states(self):
        for x in self._final_states:
            self.add_selfloop(x)

    def remove_unreachable(self):
        succ = self.succ
        actual = set([self._initial_state])
        reached = set()
        while True:
            reached = reached.union(actual)
            new = set()
            for q in actual:
                new = new.union(succ[q])
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
        out._transitions = [defaultdict(set) for x in range(cnt)]
        for state, rules in enumerate(self._transitions):
            if state in state_map:
                for key, val in rules.items():
                    out._transitions[state_map[state]][key] = set([state_map[x] \
                    for x in val if x in state_map])

        out._initial_state = state_map[self._initial_state]
        out._final_states = [state_map[x] for x in self._final_states \
                             if x in state_map]
        out._compute_depth()

        return out


    def state_error(self, state):
        sf = self._state_freq[state]
        return 0 if sf == 0 else sf / self.total_freq

    def reduce(self, error=0, depth=0):
        marked = set()
        srt_states = sorted([x for x in range(self.state_count) \
                            if self._state_depth[x] >= depth], \
                            key=lambda x : self._state_freq[x])

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
        for state, rules in enumerate(self._transitions):
            for key, val in rules.items():
                new_transitions[state][key] = \
                set([x if x not in marked else final_state_label for x in val])

        out = NetworkNFA()
        out._transitions = new_transitions
        out._final_states = self._final_states.copy()
        out._initial_state = self._initial_state
        out._final_states.add(final_state_label)
        out = out.remove_unreachable()
        sys.stderr.write('{}/{}\n'.format(out.state_count,self.state_count))

        return out

    def reduce_by_merge(self):

        pred = self.pred
        succ = self.succ

        def merge_states(self, state1, state2):
            """
            state2 -> state1
            """
            nonlocal pred
            nonlocal succ
            for p in pred[state2]:
                for ss in self.transitions[p]:
                    if state2 in ss:
                        ss.discard(state2)
                        ss.add(state1)
            for s in self.transitions[state2]:
                tmp = self.transitions[state1]
                tmp = tmp.union(s)
            self.transitions[state2].clear()

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

    a = NetworkNFA.parse_fa_file(args.input)

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
