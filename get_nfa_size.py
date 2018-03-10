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

    return (fname, str(len(states)), str(fstates), trans)

def adjust(s1,s2,s3,s4):
    print(s1.ljust(30), s2.ljust(7), s3.ljust(5), s4)

adjust('nfa','states','fin','trans')

for x in sys.argv[1:]:
    v = foo(x)
    adjust(*v)
