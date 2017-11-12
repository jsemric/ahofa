#!/usr/bin/env python3

from collections import defaultdict
import operator
import random

import nfa

def merge_random(aut, pct=0, prefix=0, suffix=0):
    # result is mapping state->state
    res = {}
    # set of states which will be collapsed
    state_depth = aut.state_depth
    states = [ state for state in aut.states if state_depth[state] > prefix ]
    if suffix:
        succ = aut.succ
        pred = aut.pred
        tmp = [i for i in aut.states if not succ[i]]
        for i in range(suffix):
            for s in tmp.copy():
                tmp |= pred[s]
        states -= tmp

    # create equivalence classes
    equiv_groups_count = int(aut.state_count * pct) - (aut.state_count - \
    len(states))

    if equiv_groups_count > 1:
        equiv_groups = [set() for x in range(equiv_groups_count)]
    else:
        # minimal number of equivalence groups
        equiv_groups = [set() for x in range(3)]
        equiv_groups_count = 3

    for state in states:
        x = random.randrange(0, equiv_groups_count)
        equiv_groups[x].add(state)

    for eq in equiv_groups:
        if len(eq) > 1:
            p = eq.pop()
            res[p] = p
            for q in eq:
                res[q] = p
                aut.merge_states(p, q)
        elif len(eq) == 1:
            p = eq.pop()
            res[p] = p

    aut.collapse_final_states()
    aut.clear_final_state_transitions()
    aut.remove_unreachable()
    return res

def one_line_reduction(aut, prefix=0, suffix=0):
    state_depth = aut.state_depth
    gen = aut.generator
    assert(gen != None)
    eq_groups = [
        set() for i in range(max(state_depth, key=lambda x : state_depth[x]))
    ]

    for state, depth in state_depth.items():
        eq_groups[depth].add(state)

    for states in eq_groups:
        if prefix > 0:
            prefix -= 1
            continue
        states -= aut._final_states
        first = None
        for state in states:
            if first == None:
                first = state
            else:
                aut.merge_states(first, state)

    aut.collapse_final_states()

def prune_freq(aut, freq, pct=0):
    marked = set()
    if pct == 0:
        marked = set([s for s in aut.states if freq[s] == 0])
    else:
        # states sorted according to its frequency
        srt_states = sorted(list(aut.states), key=lambda x : freq[x])
        # number of removed states
        cnt = aut.state_count - int(aut.state_count * pct)
        for state in srt_states:
            if cnt:
                cnt -= 1
                marked.add(state)
            else:
                break

    aut.prune(marked)

def prune_bfs(aut, depth, ratio=0):
    state_depth = aut.state_depth
    aut.prune(
        set([
            state for state in aut.states if state_depth[state] > depth
            ]))

def prune_linear(aut, pct=0.1):
    marked = set()
    # states sorted according to its depth
    depth = aut.state_depth
    srt_states = sorted(list(aut.states), key=lambda x : -depth[x])
    # number of removed states
    cnt = aut.state_count - int(aut.state_count * pct)
    for state in srt_states:
        if cnt:
            cnt -= 1
            marked.add(state)
        else:
            break

    aut.prune(marked)
