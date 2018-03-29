#!/usr/bin/env python3

import sys
from nfa import Nfa


def adjust(s1,s2,s3,s4):
    print(s1.ljust(30), s2.ljust(7), s3.ljust(5), s4)

adjust('nfa','states','fin','trans')

for x in sys.argv[1:]:
    v = Nfa.nfa_size(x)
    adjust(*v)
