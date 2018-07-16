#!/usr/bin/env python3
# NFA reduction functions

import sys, os
import tempfile
import multiprocessing
import itertools
from glob import glob
from copy import deepcopy
import networkx

from reduction import pruning, merging
from nfa import Nfa

def check_file(fname, dir=False):
    if dir == True:
        if not os.path.isdir(fname):
            raise RuntimeError('folder not found: ' + fname)
    else:
        if not os.path.isfile(fname):
            raise RuntimeError('file not found: ' + fname)


def reduce_nfa(aut, freq=None, ratio=.25, merge=True, th=.995, mf=.1):
    '''
    Approximate NFA reduction. The reduction consists of pruning and merging 
    based on packet frequency.

    Parameters
    ----------
    aut : Nfa class
        the NFA to reduce
    freq : str, None
        PCAP filename, or file with packet frequencies, or None
    ratio :
        reduction ratio
    merge :
        use merging reduction before pruning
    th :
        merging threshold
    mf :
        maximal frequency merging parameter

    Returns
    -------
    aut
        reduced NFA
    m
        the number of merged states
    '''
    m = 0
    if merge:
        cnt = aut.state_count #modified!!!!
        m = merging(aut, freq=freq, th=th, max_fr=mf)
        ratio = ratio * float(cnt) / (cnt - m)

    pruning(aut, ratio, freq=freq)
    return aut, m

def armc(aut, pcap, ratio=.25, th=.75, merge_empty=True):
    '''
    NFA reduction based on merging similar sets of prefixes.

    Parameters
    ----------
    aut : Nfa class
        the NFA to reduce
    pcap : str
        PCAP filename
    ratio :
        reduction ratio
    th :
        merging threshold
    merge_empty :
        if set, reduction merges states with empty sets together

    Returns
    -------
    aut
        reduced NFA
    m
        the number of merged states
    '''
    empty, eq = aut.get_armc_groups(pcap, th)
    
    mapping = {}
    # merge similar
    g = networkx.Graph(eq)
    for cluster in networkx.connected_component_subgraphs(g):
        l = list(cluster.nodes())
        assert len(l) > 1
        for i in l[1:]: mapping[i] = l[0]

    mapping.pop(aut._initial_state, None)
    m = len(mapping)

    if merge_empty:
        # merge states with empty sets of prefixes together
        fin = {s:f for f,ss in aut.fin_pred().items() for s in ss}
        mapping.update({s:fin[s] for s in empty if not s in aut._final_states})
        mapping.pop(aut._initial_state, None)
        aut.merge_states(mapping)
    else:
        aut.merge_states(mapping)
        freq = aut.get_freq(pcap)
        cnt = aut.state_count
        ratio = ratio * cnt / (cnt - m)
        pruning(aut, ratio, freq=freq)

    return aut, m

def reduce_eval(fa_name, *, test, train=None, ratios, merge=False, ths=[.995],
    mfs=[.1], nw=1):
    '''
    Perform several approximate reductions and store results to files.

    Parameters
    ----------
    fa_name : str
        name of the file with the NFA
    test : list
        ShellRegex expressions which matches PCAP files used for reduction error
        evaluation
    train :
        PCAP filename used for calculating packet frequency
    ratios : list
        reduction ratios
    merge :
        use merging reduction before pruning
    ths : list
        merging thresholds
    mfs : list
        maximal frequency merging parameters
    nw : int
        number of threads to run in parallel
    '''

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
    
    cname = os.path.basename(fa_name).replace('.fa','')
    orig_name = os.path.join(RED_DIR, cname + '.msfm')
    with open(orig_name,'w') as f: aut.print(f,how='msfm')
    
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
            msfm = os.path.join(RED_DIR, cname + '.' + h + '.msfm')
            if not os.path.exists(reduced): break
            idx += 1

        # save reduced nfa
        a.merge_redundant_states()
        with open(reduced,'w') as f: a.print(f)
        # save nfa in msfm format
        with open(msfm,'w') as f: a.print(f,how='msfm')

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
        eval_csv.append(Nfa.eval_accuracy(fa_name, reduced, test_data, nw=nw))

    with open(ERR_CSV, 'a') as f:
        for i in eval_csv: f.write(i)
    with open(RED_CSV, 'a') as f:
        for i in reduction_csv: f.write(i + '\n')
