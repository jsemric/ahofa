#!/usr/bin/env python3

import os
import sys
import subprocess as spc
import numpy as np
import glob
import itertools
from nfa import Nfa

cnt = 0

def exist(fname):
    if not os.path.exists(fname):
        raise RuntimeError('file not found: ' + fname )

def call(prog):
    global cnt
    sys.stderr.write('#' + str(cnt).zfill(4) + '\n')
    cnt += 1
    sys.stderr.write(prog + '\n')
    #return
    return spc.check_output(prog.split())

def main():
    # some constants related to filenames
    # automata directory
    AUT_DIR = 'min-snort'
    # file where information about error will be stored
    ERR_CSV = 'experiments/error.csv'
    # file where information about reduction will be written
    RED_CSV = 'experiments/reduction.csv'
    # place where reduced NFAs will be stored
    RED_DIR = 'experiments/nfa'
    # executables
    ERROR = './nfa_error'
    REDUCE = './reduce'
    # ['pcaps/meter*', 'pcaps/geant*', 'pcaps/week*']s
    TEST_PCAP = ['pcaps/extrasmall.pcap'] 
    TRAIN_PCAP = 'pcaps/extrasmall.pcap'
    # number of workers to run in parallel
    NW = 2
    # NFA to reduce
    AUTOMATA = ['dos.rules.fa']

    test_data = ' '.join(
        set([item for sub in TEST_PCAP for item in glob.glob(sub)]))
    train_data = TRAIN_PCAP

    assert type(train_data) == type(str())
    assert len(test_data) >= 1

    nw = str(NW)
    ratios = np.arange(0.10,0.32,0.15)
    iterations = range(0,2)
    results_error = []
    results_reduction = []

    for aut in AUTOMATA:
        target = os.path.join(AUT_DIR, aut)
        aut = aut.replace('.fa','')
        _, Tstates, _, Ttransitions = Nfa.nfa_size(target)

        for r, it in itertools.product(ratios, iterations):
            r = str(r)
            it = str(it)
            
            # creating name for reduced nfa
            idx = 0
            while True:
                h = str(idx).zfill(5)
                reduced = os.path.join(RED_DIR, aut + '.' + h + '.fa')
                if not os.path.exists(reduced): break
                idx += 1
            print(reduced)
            # reduction
            prog = ' '.join(
                [REDUCE, target, train_data, '-r', r, '-i', it, '-o', reduced])
            call(prog)

            _, Rstates, _, Rtransitions = Nfa.nfa_size(reduced)

            # target,reduced,pcap,ratio,iter,Tstates,Ttransitions,Rstates,
            # Rtransitions
            o = ','.join([str(x) for x in [
                target, reduced, train_data, r, it, Tstates, Ttransitions, 
                Rstates, Rtransitions]])
            results_reduction.append(o)

            # error
            prog = ' '.join([ERROR, target, reduced, '-n', nw, test_data])
            o = call(prog)
            o = o.decode("utf-8")
            results_error.append(o)


    with open(ERR_CSV, 'a') as f:
        for i in results_error: f.write(i)
    with open(RED_CSV, 'a') as f:
        for i in results_reduction: f.write(i + '\n')

if __name__ == "__main__":
    main()