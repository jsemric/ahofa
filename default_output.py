#!/usr/bin/env python3

import os
import re
from collections import defaultdict

# directories in file system
outdir = "data"
errdir = "errors"
freqdir = "freq"
plots = "hist"
reduced = "reduced"

def split_to_sequences(val):
    tmp = list()
    for n in sorted(val):
        if not tmp:
            pass
        elif n != tmp[-1] + 1:
            yield tmp
            tmp.clear()
        tmp.append(n)

    if tmp:
        yield tmp

def get_nfa_name(aut_name):
    '''
    nfa name syntax: [NAME].fa
    '''
    s = os.path.basename(aut_name)
    return re.sub('\.(fa$|rules)', '', s)

def get_pcap_name(pcap_name):
    '''
    pcap file name syntax: packets[NUM].pcap
    '''
    s = os.path.basename(pcap_name)
    s = re.sub('.pcap$', '', s)
    x = re.search('\d*$', s)
    return (re.sub('\d*$', '', s), x.group())

def get_freq_name(freq_name):
    '''
    nfa state frequencies syntax: [NAME][[PCAP][NUM] ...].freq
    '''
    s = os.path.basename(freq_name)
    return re.sub('\.freq$', '', s)

def reduced_fname(aut, freq, pct=0, depth=0):
    f = get_freq_name(freq)
    if pct or depth:
        f += '-'
    if pct:
        f += 'e' + str(pct)
    if depth:
        f += 'd' + str(depth)

    f += '.fa'
    d = os.path.join(outdir, reduced, get_nfa_name(aut))
    os.makedirs(d, exist_ok=True)

    return os.path.join(d, f)

def error_fname(target, aut, *pcaps):
    pd = defaultdict(set)
    for p in pcaps:
        pn, n = get_pcap_name(p)
        pd[pn].add(int(n))

    fname = ''
    for key, val in pd.items():
        fname = key
        for x in split_to_sequences(val):
            if len(x) == 1:
                fname += str(x[0])
            else:
                fname += '{}-{}'.format(x[0],x[-1])
            fname += '_'

    print(fname)
    f = get_nfa_name(aut) + '-' + fname[:-1]
    d = os.path.join(outdir, errdir, get_nfa_name(target))
    os.makedirs(d, exist_ok=True)

    return os.path.join(d, f)
