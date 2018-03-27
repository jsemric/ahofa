#!/usr/bin/env python3

import os
import sys
import subprocess as spc
import numpy as np
import glob
import itertools

# some constants
AUT_DIR = 'min-snort'
OUTCSV = 'experiments/error.csv'
RED_DIR = 'experiments/reduced'
ERROR = './nfa_error'
PCAPS = ['pcaps/meter*', 'pcaps/geant*', 'pcaps/week*']
REDUCE = './reduce'
NW = 2

def exist(fname):
    if not os.path.exists(fname):
        raise RuntimeError('file not found: ' + fname )

def call(prog):
    sys.stderr.write(prog + '\n')
    #return
    return spc.check_output(prog.split())

tmp = ''
for i in PCAPS:
    tmp += ' ' + ' '.join(f for f in glob.glob(i))
PCAPS = tmp
NW = str(NW)

#auts = ['ftp.rules.fa','imap.rules.fa','web-activex.rules.fa']
auts = ['dos.rules.fa','sprobe.fa','backdoor.rules.fa','web.php.rules.fa']
#auts = ['l7-all.fa','ex.web.rules.fa']
train_pcap = 'pcaps/geant.pcap'
ratios = np.arange(0.10,0.32,0.15)
iterations = range(0,2)

results = []

for a in auts:
    target = os.path.join(AUT_DIR, a)
    a = a.replace('.fa','')

    for r, it in itertools.product(ratios, iterations):
        r = str(r)
        it = str(it)
        
        # creating name for reduced nfa
        reduced = os.path.join(
            RED_DIR,
            '{}.{}-r{}-i{}.fa'.format(a, os.path.basename(train_pcap), r, it))

        # reduction
        prog = ' '.join(
            [REDUCE, target, train_pcap, '-r', r, '-i', it, '-o', reduced])
        call(prog)

        # error
        prog = ' '.join([ERROR, target, reduced, '-n', NW, PCAPS])
        o = call(prog)
        o = o.decode("utf-8")
        o = o.split('\n')[:-1]
        # add iteration parameter as information to csv
        o = [x + ',' + it for x in o]
        results += o

with open(OUTCSV,'a') as f:
    #print("nfa,states,states reduced,pcap,fp_a,pp_a,fp_c,pp_c,all_c,it")
    for i in results:
        f.write(i + '\n')