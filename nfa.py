#!/usr/bin/env python3
# implementation of NFA base class

import re
import math
import sys
from collections import defaultdict
import subprocess as subpr
import os
import tempfile

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
    def state_count(self):
        return len(self._transitions)

    @property
    def trans_count(self):
        return sum(len(ss) for t in self._transitions.values() \
            for ss in t.values())

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
    def alphabet(self):
        alph = set()
        for state, rules in self._transitions.items():
            for key, value in rules.items():
                alph.add(key)
        return alph


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

    def has_path_over_alph(self, state1, state2):
        alph = [1 for x in range(256)]
        for key, val in self._transitions[state1].items():
            if state2 in val:
                alph[key] = 0
            else:
                return False

        return not sum(alph)

    def extend_final_states(self):
        symbol = 257
        new_state_id = max(self.states) + 1
        new_finals = set()

        for fin in self._final_states:
            self._transitions[fin][symbol].add(new_state_id)
            self._transitions[new_state_id] = defaultdict(set)
            new_finals.add(new_state_id)
            new_state_id += 1
            symbol += 1

        self._final_states = new_finals
        return symbol - 1

    def retrieve_final_states(self):
        pred = self.pred
        final_state = self._final_states.pop()
        assert len(self._final_states) == 0

        for fin in pred[final_state]:
            #assert len(self._transitions[fin]) == 1
            self._transitions[fin] = {
                key:val for key,val in self._transitions[fin].items()
                if key < 256}
            self._final_states.add(fin)

        # remove old final state
        del self._transitions[final_state]

    def fin_pred(self):
        '''
        Find all predecessor states of final states.

        Return
        ------
        res
            dictionary mapping final states: state that from which we can reach
            that final state
        '''
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
        '''
        Read an NFA from file.

        Parameters
        ----------
        fdesc : file
            file with an NFA
        how : {'fa', 'ba'}
            how to parse the input, fa or ba format
        '''
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
        elif how == 'msfm':
            yield '{}\n'.format(self.state_count)
            yield '{}\n'.format(self.trans_count)
            yield '{}'.format(self._initial_state)

            alph_dict = {}
            i = 0
            for symbol in self.alphabet:
               alph_dict[symbol] = i
               i += 1
        else:
            raise NfaError('fa, dot or ba') # TODO

        for state, rules in self._transitions.items():
            for key, value in rules.items():
                for q in value:
                    if how == 'ba':
                        yield '{},[{}]->[{}]\n'.format(hex(key), state, q)
                    elif how == 'msfm':
                        yield '\n{}|{}|{}|0'.format(state, alph_dict[key], q) 
                    else:
                        yield '{} {} {}\n'.format(state, q, hex(key))

        if how=='msfm':
            yield '\n######################################################################################\n'
            yield '{}\n'.format(len(self._final_states))
            for qf in self._final_states:
                yield '{},'.format(qf)
            yield '\n######################################################################################\n'

            yield '{}'.format(len(alph_dict))
            for symbol, index in alph_dict.items():
                yield '\n{}:{}|'.format(index,hex(symbol))
        else:
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
        freq_scale=None,state_labels=True):
        '''
        Converts NFA to the dot format.

        Parameters
        ----------
        show_trans :
            show transition labels
        freq : dict
            state frequencies
        states : set
            which states to show
        show_diff :
            show differences between frequencies of adjacent states
        freq_scale :
            scaling function for packet frequencies
        state_labels :
            show state labels

        Return
        ------
        NFA in the dot format
        '''


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
                if state_labels:
                    yield 'node [shape={},style=filled,fillcolor="{}",label='\
                        '"{}"];q{}\n'.format(shape, color, freq[state],state)
                else:
                    yield 'node [shape={},style=filled,fillcolor="{}"'\
                        '];q{}\n'.format(shape, color, state)
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


    ###########################################################################
    # REDUCTION
    ###########################################################################

    def merge_states(self, mapping):
        '''
        Merge given states.

        Parameters
        ----------
        mapping : dict
            mapping state1:state2 where state1 is merged into state2
        '''

        if set(mapping.keys()) & set(mapping.values()):
            raise RuntimeError('merging not consistent')

        states = set(self.states)
        #mapping = {k:v for k,v in mapping.items() if k != self._initial_state}

        for p,q in mapping.items():
            if not p in states or not q in states:
                raise RuntimeError('invalid state id')
            if p == self._initial_state:
                raise RuntimeError('cannot merge initial state')

            for a,ss in self._transitions[p].items():
                self._transitions[q][a] |= ss
            del self._transitions[p]
            if p in self._final_states:
                self._final_states.add(q)

        for s,t  in self._transitions.copy().items():
            for a,ss in t.items():
                self._transitions[s][a] = set(
                    mapping[x] if x in mapping else x for x in ss)

        self._final_states -= set(mapping.keys())

    def merge_redundant_states(self):
        '''
        Simplify the NFA by merging redundant states.
        '''
        to_merge = set()
        pred = self.pred

        for s in self.succ[self._initial_state]:
            if self.has_path_over_alph(self._initial_state, s) and \
                self.has_path_over_alph(s,s) and len(pred[s]) == 2:
                to_merge.add(s)

        to_merge.discard(self._initial_state)
        if len(to_merge) > 1:
            print(len(to_merge))
            p = to_merge.pop()
            self.merge_states({q:p for q in to_merge})

    def compute_freq(self, pcap):
        '''
        Call external program for calculating packet frequency for the NFA.

        Parameters
        ----------
        pcap :
            PCAP filename

        Return
        ------
        dictionary containing state : frequency
        '''
        fa_file = tempfile.NamedTemporaryFile()
        fr_file = tempfile.NamedTemporaryFile()
        with open(fa_file.name, 'w') as f:
            self.print(f)
        subpr.call(['{}./state_frequency'.format(os.getenv('AHOFA_PATH')), fa_file.name, pcap, fr_file.name])
        return self.retrieve_freq(fr_file.name)

    def retrieve_freq(self, fname):
        '''
        Read frequencies from line-based file with the following syntax:

        <state> <freq>
        ...

        The state is the state of the NFA (labels must match), and freq
        is the corresponding packet frequency.

        Parameters
        ----------
        fname:
            filename

        Return
        ------
        dictionary containing state : frequency
        '''
        freq = {}
        with open(fname, 'r') as f:
            for line in f:
                line = line.split('#')[0]
                if line:
                    state, fr, *_ = line.split()
                    freq[int(state)] = int(fr)

        if set(freq.keys()) != set(self.states):
            raise RuntimeError('failed to read state frequencies')

        return freq

    @classmethod
    def eval_accuracy(cls, target, reduced, pcap, *, nw=1):
        '''
        Call external program for error evaluation of the reduced NFA.

        Parameters
        ----------
        target:
            file with the original NFA
        reduced:
            file with the reduced NFA
        pcap: str
            PCAP filenames, separated by spaces
        nw:
            number of threads to run in parallel

        Return
        ------
        string containing the values of evaluation statistics separated by a
        comma
        '''
        prog = ' '.join(['{}./nfa_eval'.format(os.getenv('AHOFA_PATH')), target, reduced, '-n', str(nw), pcap,
             '-c']).split()
        sys.stderr.write('{}\n'.format(prog))
        o = subpr.check_output(prog)
        return o.decode("utf-8")


    def get_freq(self, fname=None, freq_file=False, subtract=False):
        '''
        Get packet frequencies.

        Parameters
        ----------
        fname:
            filename of PCAP file or file with state frequencies
        freq_file:
            if set fname is considered to be file containing frequencies, not a
            PCAP file
        subtract:
            if set, the states frequencies are subtracted by the packet
            frequencies of reachable final states
       
        Return
        ------
        dictionary containing state : frequency
        '''
        if fname == None:
            freq = {s:0 for s in self.states}
        elif freq_file:
            freq = self.retrieve_freq(fname)
        else:
            freq = self.compute_freq(fname)

        if subtract:
            for f,ss in self.fin_pred().items():
                for s in ss:
                    freq[s] =  max(freq[s] - freq[f], 0)

        return freq

    def get_armc_groups(self, pcap, th=.5):
        '''
        Call external program for computing similar sets of prefixes.

        Parameters
        ----------
        pcap :
            PCAP file
        th :
            similarity threshold

        Return
        ------
        empty:
            array of state which have empty sets of prefixes
        sim:
            array of pairs of states having similar prefixes (excluding states 
            with empty sets)
        '''
        fa_file = tempfile.NamedTemporaryFile()
        with open(fa_file.name, 'w') as f: self.print(f)
        out = subpr.check_output('./prefix_labeling {} {} {}'.format(
            fa_file.name, pcap, th).split()).decode('utf-8').split('\n')
        empty = [int(s) for s in out[0].split()]
        #print(empty)
        sim = []
        for ss in out[1:-1]:
            p, q = ss.split()
            sim.append((int(p),int(q)))
        return empty, sim
