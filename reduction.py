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
class GradientReduction(PruneReduction):
    def __init__(self, automaton, ratio, *, error=False):
        super().__init__(automaton, ratio, error=error)

    def reduce(self, *, verbose=False):
        succ = self._nfa.succ
        freq = self._mapping
        for s in self._nfa.states:
'''

class BigramReduction(PruneReduction):
    def __init__(self, automaton, ratio, *, error=False):
        super().__init__(automaton, ratio, error=error)

    def evaluate_states(self, filename):
        with open(filename, 'r') as f:
            mx = np.loadtxt(f, delimiter=' ')
        mx = mx / mx.sum()
        self._mapping = self._nfa.eval_states(mx)

    def reduce(self, *, verbose=False):
        depth = self._nfa.state_depth
        finals = self._nfa._final_states
        mapping = self._mapping
        gen = self._nfa.generator
        pred = self._nfa.pred
        rules = []
        rule_sums = []

        for i in self._nfa.split_to_rules().values():
            srt_rule = sorted(i, key=lambda x: (depth[x], len(pred[x] & i)))
            fin = next(x for x in i if x in finals)
            srt_rule.pop(srt_rule.index(fin))
            vals = [ mapping[x] for x in srt_rule ]
            rules.append((fin, srt_rule))
            rule_sums.append(sum(vals))

        merge_mapping = dict()
        max_to_remove = len(rules) * 3 + 2
        to_remove = max(max_to_remove,self._nfa.state_count * (1-self._ratio))
        
        while to_remove > 0:
            _max = max(rule_sums)
            index = rule_sums.index(_max)
            rule = rules[index]
            print(_max, rule[0])
            #print(rule)
            #len(rule_to_shrink) / 2
            first = rule[1][0]
            last = rule[1][-1]
            if mapping[first] > mapping[last]:
                rule_sums[index] -= mapping[first]
                merge_mapping[first] = gen
                rule[1].pop(0)
            else:
                rule_sums[index] -= mapping[last]
                merge_mapping[last] = rule[0]
                rule[1].pop()

            to_remove -= 1
        print('=====')
        for i in rule_sums:
            print(i)


        self._nfa.merge_states(merge_mapping,clear_finals=False)

