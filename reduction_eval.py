#!/usr/bin/env python3

import sys, os
import tempfile
import multiprocessing
import itertools
from glob import glob
from copy import deepcopy

from reduction import prunning, merging
from nfa import Nfa

def check_file(fname, dir=False):
    if dir == True:
        if not os.path.isdir(fname):
            raise RuntimeError('folder not found: ' + fname)
    else:
        if not os.path.isfile(fname):
            raise RuntimeError('file not found: ' + fname)


def reduce_nfa(aut, freq=None, ratio=.25, merge=True, th=.995, mf=.1):
    m = 0
    if merge:
        m = merging(aut, freq=freq, th=th, max_fr=mf)
        cnt = aut.state_count
        ratio = ratio * cnt / (cnt - m)

    prunning(aut, ratio, freq=freq)
    return aut, m

def reduce_eval(fa_name, *, test, train=None, ratios, merge=False, ths=[.995],
    mfs=[.1], nw=1):

    RED_DIR = 'experiments/nfa'
    ERR_CSV = 'experiments/eval.csv'
    RED_CSV = 'experiments/reduction.csv'

    if not merge:
        ths, mfs = [None], [None]

    test_data = ' '.join(set([item for sub in test for item in glob(sub)]))

    assert len(test_data) >= 1
    assert 1 <= nw <= multiprocessing.cpu_count()
    for i in test_data.split(): check_file(i)
    for i in ['state_frequency', 'nfa_eval']: check_file(i)
    check_file(RED_DIR, True)
    for i in ratios: assert 0.0001 < i < 0.99

    aut = Nfa.parse(fa_name)
    freq = aut.get_freq(train)
    reduction_csv = []
    eval_csv = []

    for r, th, mf in itertools.product(ratios, ths, mfs):
        a, m = reduce_nfa(deepcopy(aut), freq, r, merge, th, mf)
        # save reduction data
        cname = os.path.basename(fa_name).replace('.fa','')
        idx = 0
        while True:
            h = str(idx).zfill(5)
            reduced = os.path.join(RED_DIR, cname + '.' + h + '.fa')
            if not os.path.exists(reduced): break
            idx += 1

        # save reduced nfa
        with open(reduced,'w') as f: a.print(f)
        # store reduction result to csv
        pname = str(train)

        cname = os.path.basename(reduced).replace('.fa','')
        if merge:
            o = ','.join([str(x) for x in [cname, os.path.basename(pname), r,
                th, mf, m, a.state_count, a.trans_count]])
        else:
            o = ','.join([str(x) for x in [cname, os.path.basename(pname), r,
                'NA', 'NA', 0, a.state_count, a.trans_count]])
        reduction_csv.append(o)

        # eval error and save result
        #eval_csv.append(Nfa.eval_accuracy(fa_name, reduced, test_data, nw=nw))

    #with open(ERR_CSV, 'a') as f:
    #    for i in eval_csv: f.write(i)
    with open(RED_CSV, 'a') as f:
        for i in reduction_csv: f.write(i + '\n')
