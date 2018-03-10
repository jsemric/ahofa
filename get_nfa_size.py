#!/usr/bin/env python3

import sys

def foo(fname):
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

    print(fname,len(states),trans,fstates)




for x in sys.argv[1:]:
    foo(x)
    '''
    a = Nfa.parse(x)
    print(x)
    trans_cnt = 0
    for k,v in a._transitions.items():
        trans_cnt += len(k)
    print(a.state_count, len(a._final_states))
    '''
