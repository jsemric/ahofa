#!/usr/bin/env python3

import numpy as np
from nfa import Nfa
from nfa import NfaError

class PruneReduction():

    def __init__(self, automaton, ratio, *, error=False):
        # error or reduction ratio
        self._ratio = ratio
        self._error = error
        self._nfa = automaton
        self._mapping = dict()
        if not 0 < ratio < 1:
            raise NfaError('invalid reduction ratio: ' + str(ratio))

    def evaluate_states(self, filename):
        with open(filename) as f:
            for line in f:
                line = line.split('#')[0]
                if line:
                    state, freq, *_ = line.split()
                    self._mapping[int(state)] = int(freq)

    def reduce(self, *, verbose=False):
        if len(self._mapping) == 0:
            raise NfaError('cannot reduce without evaluating states before')

        state_count = self._nfa.state_count
        mapping = self._mapping
        finals = self._nfa._final_states
        depth = self._nfa.state_depth

        state_importance = sorted(
            [ x for x in mapping.keys() if not x in finals ],
            key=lambda x: (mapping[x], -depth[x]))
 
        rules = self._nfa.split_to_rules()
        max_to_remove = len(rules) * 3 + 2
        # states to prune
        to_prune = set()
        removed = 0
    
        if self._error:
            # reduce with regard to reduction error parameter
            _max = max(mapping.values())
            mapping = { key : val / _max for key,val in mapping.items() }
            
            error = self._ratio
            pct = 0
            while removed < max_to_remove and pct < error:
                pct += mapping[state_importance[removed]]
                removed += 1
        else:
            # reduce with regard to reduction ratio parameter
            removed = int((1 - self._ratio) * state_count)
            removed = max(removed, max_to_remove)
        
        to_prune = set(state_importance[:removed])

        # map removed states to final states
        merge_mapping = dict()
        for rule in rules.values():
            fin = None
            for state in rule:
                if state in finals:
                    fin = state
                    break
            assert fin != None
            for s in rule:
                if s in to_prune:
                    merge_mapping[s] = fin

        self._nfa.merge_states(merge_mapping,clear_finals=False)
        # adjust member variables
'''
obsolete
class CorrReduction(PruneReduction):
    def __init__(self, automaton, ratio, *, error=False):
        super().__init__(automaton, ratio, error=True)

    def evaluate_states(self, filename):
        with open(filename, 'r') as f:
            mx = np.loadtxt(f, delimiter=' ')
        mx = np.log(mx / mx.max())
        self._mapping = self._nfa.eval_states(mx)
'''