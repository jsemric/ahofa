#!/usr/bin/env python3

import sys, os
import matplotlib.pyplot as plt
from collections import defaultdict
from nfa import Nfa

def makepie(fname,pos):
    nfa = Nfa.parse(x)
    dd = defaultdict(int)
    for i in nfa.neigh_count().values(): dd[i] += 1

    _min = max(dd.values()) / 30
    dc = []
    lab = []
    other = 0
    for key,val in dd.items():
        if val > _min:
            dc.append(val)
            lab.append(key)
        else:
            other += val

    if other != 0:
        if other < _min and len(lab) > 2:
            dc[-1] += other
            lab[-1] = 'others'
        else:
            dc.append(other)
            lab.append('others')
    ex = [0.15]*len(dc)
 #   ex[1] = 0.2

    plt.subplot(pos)
    plt.pie(dc,labels=lab,autopct='%1.1f%%',explode=ex,shadow=1)
    plt.title(os.path.basename(fname).replace('.fa',''))

pos=221
for x in sys.argv[1:]:
    makepie(x,pos)
    pos+=1

plt.show()
