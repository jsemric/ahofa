#!/usr/bin/env python3

import os
import sys
import subprocess as spc
import numpy as np
import glob

# some constants
NOR_DIR = 'snort'
AUT_DIR = 'min-snort'
FRQ_DIR = 'data/freq'
ERR_DIR = 'data/error'
RED_DIR = 'data/reduced'
ERROR = './nfa_error'
PCAPS = 'pcaps/meter*'
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

#auts = ['ex.web.rules.fa','web-activex.rules.fa']
auts=[]
ratios = np.arange(0.10,0.32,0.02)
freq_pcap = ['pcaps/geant.pcap']

for a in auts:
    spath = os.path.join(NOR_DIR, a)
    apath = os.path.join(AUT_DIR, a)

    # comment out if already minimized
    call('./lmin {} {}'.format(spath, apath))

    a = a.replace('.fa','')
    pcap_name = ''
    for x in freq_pcap:
        pcap_name += os.path.basename(x.replace('.pcap',''))

    freq = os.path.join(FRQ_DIR, a + '-' + pcap_name)

    # comment out if frequency has been already computed
    prog = '{} -f -o {} {} '.format(REDUCE, freq, apath) + ' '.join(freq_pcap)
    call(prog)

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
