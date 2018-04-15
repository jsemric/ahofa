#!/usr/bin/env python3

import re
import math
import sys
from collections import defaultdict

def rgb(maximum, minimum, value):
    minimum, maximum = float(minimum), float(maximum)
    ratio = 2 * (value-minimum) / (maximum - minimum)
    b = int(max(0, 255*(1 - ratio)))
    r = int(max(0, 255*(ratio - 1)))
    g = 255 - b - r
    return r, g, b

def sanitize_labels(labels):
    if len(labels) == 0:
        return "e"
    elif len(labels) == 1:
        return hex(labels[0])

    last = labels[0]
    res = hex(labels[0])
    in_seq = 0
    for i in labels[1:]:
        if last == i - 1:
            in_seq += 1
        else:
            if in_seq > 0:
                res += '-' + hex(last)
            in_seq = 0
            res += ',' + hex(i)
        last = i

    if in_seq:
        res += '-' + hex(last)

    return res

class NfaError(Exception):
    pass

class Nfa:

    # regex for the parsing
    regex_trans_fa = re.compile('^\w+\s+\w+\s+\w+$')
    regex_state_fa = re.compile('^\w+$')
    regex_trans_ba = re.compile('^\w+,\s*\[\w+\]->\s*\[\w+\]$')
    regex_state_ba = re.compile('^\[\w+\]$')

    @classmethod
    def nfa_size(cls, fname):
        with open(fname, 'r') as f:
            for line in f: break
            trans = 0
            fstates = 0
            states = set()
            for line in f:
                x = line.split(' ')
                if len(x) == 1:
                    fstates += 1
                    break
                states.add(x[0])
                states.add(x[1])
                trans += 1

            for line in f:
                fstates += 1

        return (fname, str(len(states)), str(fstates), trans)

    def __init__(self):
        self._transitions = dict(defaultdict(set))
        self._initial_state = None
        self._final_states = set()

    ###########################################################################
    # PROPERTIES
    ###########################################################################

    @property
    def generator(self):
        ret = set()
        for s in self.succ[self._initial_state]:
            if self._has_path_over_alph(s, s):
                ret.add(s)
        if len(ret) == 1:
            return ret.pop()
        else:
            return ret

    @property
    def state_count(self):
        return len(self._transitions)

    @property
    def states(self):
        yield from self._transitions.keys()

    @property
    def pred(self):
        pred = {s:set() for s in self._transitions}

        for state, rules in self._transitions.items():
            for key, value in rules.items():
                for q in value:
                    pred[q].add(state)

        return pred

    @property
    def succ(self):
        succ = {s:set() for s in self._transitions}

        for state, rules in self._transitions.items():
            for key, value in rules.items():
                for q in value:
                    succ[state].add(q)

        return succ

    @property
    def state_depth(self):
        succ = self.succ
        sdepth = {state:0 for state in self.states}
        actual = set([self._initial_state])
        empty = set()
        depth = 0
        while True:
            empty = empty.union(actual)
            new = set()
            for q in actual:
                sdepth[q] = depth
                new = new.union(succ[q])
            new -= empty
            actual = new
            if not new:
                break
            depth += 1
        return sdepth

    ###########################################################################
    # NFA SETTER METHODS
    ###########################################################################

    def _add_state(self,state):
        if not state in self._transitions:
            self._transitions[state] = (defaultdict(set))

    def _add_rule(self, pstate, qstate, symbol):
        self._add_state(pstate)
        self._add_state(qstate)
        self._transitions[pstate][symbol].add(qstate)


    def _add_initial_state(self, istate):
        self._add_state(istate)
        self._initial_state = istate

    def _add_final_state(self, fstate):
        self._add_state(fstate)
        self._final_states.add(fstate)

    ###########################################################################
    # NFA MANUPULATION, STATS, ETC.
    ###########################################################################

    def selfloop_to_finals(self):
        for state in self._final_states:
            for c in range(256):
                self._transitions[state][c] = set([state])

    def _has_path_over_alph(self, state1, state2):
        alph = [1 for x in range(256)]
        for key, val in self._transitions[state1].items():
            if state2 in val:
                alph[key] = 0
            else:
                return False

        return not sum(alph)

    def split_to_rules(self):
        res = dict()
        pred = self.pred
        for f in self._final_states:
            actual = set([f])
            visited = set()
            while actual:
                visited |= actual
                new = set()
                for q in actual:
                    new = new.union(pred[q])
                actual = new - visited
            res[f] = visited.copy()

        return res

    def neigh_count(self, selfloops=False):
        dc = {}

        for state, suc in self.succ.items():
            if selfloops == False:
                suc.discard(state)
            dc[state] = len(suc)

        return dc

    ###########################################################################
    # IO METHODS
    ###########################################################################

    @classmethod
    def parse(cls, fname, how='fa'):
        if not how in ['fa','ba']:
            raise NfaError('invalid nfa format')
        out = Nfa()
        with open(fname, 'r') as f:
            out.read(f, how)

        return out

    def read(self, fdesc, how='fa'):
        rules = 0
        trans_regex = getattr(Nfa, 'regex_trans_' + how)
        state_regex = getattr(Nfa, 'regex_state_' + how)
        for line in fdesc:
            # erase new line at the end of the string
            if line[-1] == '\n':
                line = line[:-1]

            if rules == 0:
                # read initial state
                if state_regex.match(line):
                    if how == 'ba':
                        line = re.sub('[\[\]]', '', line)
                    self._add_initial_state(int(line))
                    rules = 1
                else:
                    raise RuntimeError('invalid syntax: \"' + line + '\"')
            elif rules == 1:
                # read transitions
                if trans_regex.match(line):
                    if how == 'ba':
                        line = re.sub('[\[\,\->\]]', ' ', line)
                        a, p, q = line.split()
                    else:
                        p, q, a = line.split()
                    p = int(p)
                    q = int(q)
                    a = int(a,0)
                    self._add_rule(p,q,a)
                elif state_regex.match(line):
                    if how == 'ba':
                        line = re.sub('[\[\]]', '', line)
                    self._add_final_state(int(line))
                    rules = 2
                else:
                    raise RuntimeError('invalid syntax: \"' + line + '\"')
            else:
                if state_regex.match(line):
                    if how == 'ba':
                        line = re.sub('[\[\]]', '', line)
                    self._add_final_state(int(line))
                else:
                    raise RuntimeError('invalid syntax: \"' + line + '\"')

    def write(self, *, how='fa'):
        if how == 'fa':
            yield str(self._initial_state) + '\n'
        elif how == 'ba':
            yield '[' + str(self._initial_state) + ']\n'
        else:
            raise NfaError('fa, dot or ba') # TODO

        for state, rules in self._transitions.items():
            for key, value in rules.items():
                for q in value:
                    if how == 'ba':
                        yield '{},[{}]->[{}]\n'.format(hex(key), state, q)
                    else:
                        yield '{} {} {}\n'.format(state, q, hex(key))

        for qf in self._final_states:
            if how == 'ba':
                yield '[{}]\n'.format(qf)
            else:
                yield '{}\n'.format(qf)

    def print(self, f=None, *, how='fa'):
        for line in self.write(how=how):
            print(line, end='', file=f)

    def write_dot(
        self, *, show_trans=False, freq=None, states=None, show_diff=False,
        freq_scale=None):

        succ = self.succ

        if states == None:
            states = set(self.states)

        yield 'digraph NFA {\n \
        rankdir=LR;size="8,5"\n \
        graph [ dpi = 1000 ]\n'

        # display frequencies as a heat map
        if freq:
            freq[self._initial_state] = max(freq.values())
            if freq_scale:
                heatmap = {
                    state:int(freq_scale(f)) for state, f in freq.items()}
            else:
                heatmap = freq
            _max = max(heatmap.values())
            _min = min(heatmap.values())
            heatmap[self._initial_state] = _max
            for state in states:
                if state in self._final_states:
                    shape = "doublecircle"
                else:
                    shape = 'circle'
                r,g,b = rgb(_max, _min, heatmap[state])
                color = "#%0.2X%0.2X%0.2X" % (r, g, b)
                yield 'node [shape={},style=filled,fillcolor="{}",label="{}"];'\
                    'q{}\n'.format(shape, color, freq[state],state)
        else:
            yield '{node [shape = doublecircle, style=filled, fillcolor=red];'
            yield ';'.join(['q' + str(qf) for qf in self._final_states & states]) + '\n'
            yield '}\n'

        # initial state
        yield 'node [shape = point]; qi\nnode [shape = circle];\n'
        yield 'qi -> q' + str(self._initial_state) + ';\n'

        # display transitions
        for state in states:
            for s in succ[state]:
                if not s in states:
                    continue
                yield ' '.join(('q' + str(state), '->', 'q' + str(s)))
                # display labels
                if show_trans:
                    labels = []
                    for symbol, _states in self._transitions[state].items():
                        if s in _states:
                            labels.append(symbol)
                    yield ' [ label="' + sanitize_labels(labels) + '"]'
                elif show_diff:
                    if freq == None:
                        raise RuntimeError('freq argument must be specified')
                    if freq[state] > 0:
                        diff = round(100*freq[s]/freq[state], 2)
                        yield ' [ label="' + str(diff) + '%"]'
                yield ";\n"

        yield '}\n'

    def print_dot(self, f=None):
        for line in self.write_dot():
            print(line, end='', file=f)
