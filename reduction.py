#!/usr/bin/env python3
# NFA reduction algorithms: pruning and merging

import networkx

from nfa import Nfa

def pruning(aut, ratio=.25, *, freq):
    '''
    Pruning NFA reduction (in place).

    Parameters
    ----------
    aut : Nfa class
        the NFA to reduce
    ratio :
        reduction ratio
    freq : str, None
        PCAP filename, or file with packet frequencies, or None
    
    '''
    #if not 0 < ratio < 1:
    #    raise RuntimeError('invalid reduction ratio value: ' + str(ratio))

    depth = aut.state_depth
    states = set(aut.states) - aut._final_states - set([aut._initial_state])
    states = sorted(states, key=lambda x: (freq[x], -depth[x]), reverse=True)

    orig_cnt = aut.state_count
    cnt = int(round(ratio * orig_cnt) - len(aut._final_states) - 1)
    assert cnt > 1
    fin = {}

    fin = {s:f for f,ss in aut.fin_pred().items() for s in ss}
    mapping = {s:fin[s] for s in states[cnt:]}
    
    aut.merge_states(mapping)


def merging(aut, *, th=.995, max_fr=.1, freq=None):
    '''
    Merging NFA reduction (in place).

    Parameters
    ----------
    aut : Nfa class
        the NFA to reduce
    freq : str, None
        PCAP filename, or file with packet frequencies, or None
    th :
        merging threshold
    mf :
        maximal frequency merging parameter

    Returns
    -------
    m
        the number of merged states
    '''    
    if freq == None: raise RuntimeError('packet frequency not provided')
    if not 0 <= th <= 1: raise RuntimeError('invalid threshold value')
    if not 0 <= max_fr <= 1: raise RuntimeError('invalid max_fr value')

    succ = aut.succ
    actual = set([aut._initial_state])
    visited = set([aut._initial_state])
    finals = aut._final_states

    mapping = {}
    marked = []
    max_ = max_fr * max(freq.values())
    # BFS
    while actual:
        new = set()
        for p in actual:
            freq_p = freq[p]
            t = freq_p / max_
            if not p in finals and freq_p != 0 and t <= max_fr:
                for q in succ[p] - finals - set([p]):
                    freq_q = freq[q]
                    d = min(freq_q,freq_p) / max(freq_q,freq_p)
                    if d > th and p != aut._initial_state: marked.append((p,q))

            new |= succ[p]
        actual = new - visited
        visited |= new
    
    # handle transitivity
    g = networkx.Graph(marked)
    for cluster in connected_component_subgraphs(g):
        l = list(cluster.nodes())
        assert len(l) > 1
        for i in l[1:]: mapping[i] = l[0]
    
    aut.merge_states(mapping)
    # return the number of merged (removed) states
    return len(mapping)

def connected_component_subgraphs(G):
    for c in networkx.connected_components(G):
        yield G.subgraph(c)
