#!/usr/bin/env python3

import os
import sys
import subprocess as spc
import numpy as np
import glob

# some constants
AUT_DIR = 'min-snort'
FRQ_DIR = 'data/freq'
ERR_DIR = 'data/error'
RED_DIR = 'data/reduced'
ERROR = './nfa_error'
PCAPS = 'pcaps/*k.pcap'
REDUCE = './reduce'
NW = 2

def exist(fname):
    if not os.path.exists(fname):
        raise RuntimeError('file not found: ' + fname )

def call(prog):
    sys.stderr.write(prog + '\n')
    #return
    spc.call(prog.split())

PCAPS = ' '.join(f for f in glob.glob(PCAPS))
NW = str(NW)

auts = ['backdoor.rules.fa']
#ratios = np.arange(0.14,0.4,0.02)
ratios = np.arange(0.14,0.16,0.02)
freq_pcap = ['pcaps/20k.pcap']

for a in auts:
    apath = os.path.join(AUT_DIR, a)
    a = a.replace('.fa','')
    pcap_name = ''
    for x in freq_pcap:
        pcap_name += os.path.basename(x.replace('.pcap',''))

    freq = os.path.join(FRQ_DIR, a + '-' + pcap_name)
    prog = '{} -f -o {} {} '.format(REDUCE, freq,apath) + ' '.join(freq_pcap)
    call(prog)
    # state frequency
    # spc.call(prog.split())
    for r in ratios:
        r = str(r)
        # pruning
        out1 = os.path.join(
            RED_DIR, os.path.basename(freq) + '-prune-r' + r + '.fa')

        prog = ' '.join(
            [REDUCE, apath, freq, '-p', r, '-t prune', '-o', out1])
        call(prog)

        # merging
        out2 = os.path.join(
            RED_DIR, os.path.basename(freq) + '-merge-r' + r + '.fa')

        prog = ' '.join(
            [REDUCE, apath, freq, '-p', r, '-t merge', '-o', out2])
        call(prog)

        # error pruning
        prog = ' '.join(
            [ERROR, apath, out1, '-j -s', '-n', NW, '-o', ERR_DIR, PCAPS])
        call(prog)

        # error merging
        prog = ' '.join(
            [ERROR, apath, out2, '-j -s', '-n', NW, '-o', ERR_DIR, PCAPS])
        call(prog)
