#!/usr/bin/env python3

from nfa import Nfa

def prunning(aut, ratio=.25, *, freq):
    if not 0 < ratio < 1:
        raise RuntimeError('invalid reduction ratio value: ' + str(ratio))

    depth = aut.state_depth
    states = set(aut.states) - aut._final_states - set([aut._initial_state])
    states = sorted(states, key=lambda x: (freq[x], -depth[x]), reverse=True)

    orig_cnt = aut.state_count
    cnt = int(round(ratio * orig_cnt) - len(aut._final_states) - 1)
    assert cnt > 1
    fin = {}

    for f,ss in aut.fin_pred().items():
        for s in ss:
            fin[s] = f
    mapping = {s:fin[s] for s in states[cnt:]}
    
    aut.merge_states(mapping)


def merging(aut, *, th=.995, max_fr=.1, freq=None):
    if freq == None: raise RuntimeError('packet frequency not provided')
    if not 0 <= th <= 1: raise RuntimeError('invalid threshold value')
    if not 0 <= max_fr <= 1: raise RuntimeError('invalid max_fr value')

    succ = aut.succ
    actual = set([aut._initial_state])
    visited = set([aut._initial_state])
    finals = aut._final_states

    mapping = {}
    max_ = max_fr * max(freq.values())
    # BFS
    while actual:
        new = set()
        for p in actual:
            freq_p = freq[p]
            t = freq_p / max_
            if not p in finals and freq_p != 0 and t < max_fr:
                for q in succ[p] - finals - visited:
                    d = freq[q] / freq_p
                    if d > th:
                        if p in mapping:
                            mapping[q] = mapping[p]
                        else:
                            mapping[q] = p
            new |= succ[p]
        actual = new - visited
        visited |= new

    aut.merge_states(mapping)
    # return the number of merged states
    return len(mapping)