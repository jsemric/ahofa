#!/usr/bin/env python3

import os
import sys
import subprocess as spc
import numpy as np
import glob
import itertools
import multiprocessing
import argparse

from nfa import Nfa

cnt = 0

def check_file(fname, dir=False):
    if dir == True:
        if not os.path.isdir(fname):
            raise RuntimeError('folder not found: ' + fname)
    else:
        if not os.path.isfile(fname):
            raise RuntimeError('file not found: ' + fname)

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
    AUT_DIR = 'automata'
    # file where information about error will be stored
    ERR_CSV = 'experiments/error.csv'
    # file where information about reduction will be written
    RED_CSV = 'experiments/reduction.csv'
    # place where reduced NFAs will be stored
    RED_DIR = 'experiments/nfa'
    # executables
    ERROR = './nfa_eval'
    REDUCE = './reduce'

    parser = argparse.ArgumentParser()
    parser.add_argument('-r','--ratios', nargs=3, metavar='N', type=float,
        default=[0.1, 0.32, 0.2], help='reduction ratios (1st,last,step)')
    parser.add_argument('-i','--iter', type=int, metavar='N',
        default=4, help='number of iterations')
    parser.add_argument('-f','--fa', nargs='+', type=str,
        default=['backdoor.rules.fa'],
        help='automata to reduce (just basenames!)')
    parser.add_argument('-n','--nw', type=int,
        default=multiprocessing.cpu_count() - 1,
        help='number of workers to run in parallel')
    parser.add_argument('--test', type=str, nargs='+', metavar='EXP',
        default=['pcaps/meter*', 'pcaps/geant*', 'pcaps/week*'],
        help='test pcap files (shellregex possible)')
    parser.add_argument('--train', type=str, default='pcaps/geant.pcap',
        metavar='FILE', help='one train pcap file')
    args = parser.parse_args()

    test_data = ' '.join(
        set([item for sub in args.test for item in glob.glob(sub)]))
    train_data = args.train

    # check parameters
    assert type(train_data) == type(str())
    assert len(test_data) >= 1
    assert 1 <= args.nw <= multiprocessing.cpu_count()
    for i in test_data.split(): check_file(i)
    for i in [train_data, ERROR, REDUCE]: check_file(i)
    for i in [RED_DIR, AUT_DIR]: check_file(i, True)
    for i in args.fa: check_file(os.path.join(AUT_DIR, i))
    for i in args.ratios: assert 0.0001 < i < 0.99
    assert 0 < args.iter < 17

    nw = str(args.nw)
    ratios = np.arange(*args.ratios)
    iterations = range(0, args.iter)

    results_error = []
    results_reduction = []

    for aut in args.fa:
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

            # reduction
            prog = ' '.join([REDUCE, target, train_data, '-r', r, '-i', it,
                '-o', reduced])
            call(prog)

            _, Rstates, _, Rtransitions = Nfa.nfa_size(reduced)

            # reduced,pcap,ratio,iter,Tstates,Ttransitions,Rstates,
            # Rtransitions
            cname = os.path.basename(reduced.replace('.fa',''))
            o = ','.join([str(x) for x in [
                cname, os.path.basename(train_data), r, it, Tstates,
                Ttransitions, Rstates, Rtransitions]])
            results_reduction.append(o)

            # error
            prog = ' '.join([ERROR, target, reduced, '-n', nw, test_data,'-c'])
            o = call(prog)
            o = o.decode("utf-8")
            results_error.append(o)

    with open(ERR_CSV, 'a') as f:
        for i in results_error: f.write(i)
    with open(RED_CSV, 'a') as f:
        for i in results_reduction: f.write(i + '\n')

if __name__ == "__main__":
    main()
