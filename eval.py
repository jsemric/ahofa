#!/usr/bin/env python3

import numpy as np
from copy import deepcopy
import os

from nfa import Nfa
from reduction_eval import reduce_eval, armc, reduce_nfa

def eval_nfa(target, fname, test, train, method, args):
    a = Nfa.parse(target)
    _, m = method(a, train, **args)
    with open(fname, 'w') as f: a.print(f)
    r = Nfa.eval_accuracy(target, fname, test)
    _,_,total, _, _, fp, tp = r.split(',')
    print(fname,round(int(fp) / int(total), 4),m,sep=',')

def armc_vs_merge_vs_prune():
    ratio = .2
    target = 'automata/sprobe.fa'
    train = 'pcaps/10k.pcap'
    test = 'pcaps/40k.pcap'
    freq = Nfa.parse(target).get_freq(train)
    res_dir = 'experiments/armc'

    eval_nfa(target, os.path.join(res_dir,'armc_bare_th7.fa'), test, train,
        armc, {'ratio':ratio, 'th':.7, 'prune_empty':1})
    eval_nfa(target, os.path.join(res_dir,'armc_prune_th7.fa'), test, train,
        armc, {'ratio':ratio, 'th':.7, 'prune_empty':0})
    eval_nfa(target, os.path.join(res_dir,'armc_prune_th5.fa'), test, train,
        armc, {'ratio':ratio, 'th':.7, 'prune_empty':0})
    eval_nfa(target, os.path.join(res_dir,'armc_prune_th1.fa'), test, train,
        armc, {'ratio':ratio, 'th':.1, 'prune_empty':0})

    eval_nfa(target, os.path.join(res_dir,'prune.fa'), test, freq, reduce_nfa,
        {'ratio':ratio, 'merge':0})
    eval_nfa(target, os.path.join(res_dir,'merge.fa'), test, freq, reduce_nfa,
        {'ratio':ratio, 'merge':1})

def prune_cross_val():
    pass

def reduce_many():
    test = ['/home/vojta/pcap-sampled/aconet-sampled-div[1-9]']#'/home/vojta/pcaps/aconet-div121','/home/vojta/pcaps/aconet-div131','/home/vojta/pcaps/aconet-div141','/home/vojta/pcaps/aconet-div151']#,'/home/vojta/pcaps/aconet-div161','/home/vojta/pcaps/aconet-div171','/home/vojta/pcaps/aconet-div181','/home/vojta/pcaps/aconet-div191','/home/vojta/pcaps/aconet-div201']
    train = '/home/vojta/pcap-sampled/aconet-sampled-div'
    nw = 2 #number of cores, maximum on pclengal: 8

    '''
    ratios = np.arange(.14, 0.32, .02)
    reduce_eval('automata/sprobe.fa', test=test, ratios=ratios, nw=nw)
    reduce_eval('automata/sprobe.fa', test=test, train=train, ratios=ratios,
        nw=nw)
    reduce_eval('automata/sprobe.fa', test=test, train=train, ratios=ratios,
        merge=True, nw=nw)
    '''

    ratios = np.arange(.10, 0.41, .025)
    reduce_eval('automata/backdoor.rules.fa', test=test, ratios=ratios, nw=nw)
    reduce_eval('automata/backdoor.rules.fa', test=test, train=train,
        ratios=ratios, nw=nw)
    reduce_eval('automata/backdoor.rules.fa', test=test, train=train,
        ratios=ratios, merge=True,nw=nw)


    return

    # try various merging parameters
    ratios = [.18]
    mfs = np.linspace(.1,1.0,8)
    ths = np.linspace(.2,.995,10)
    reduce_eval('automata/backdoor.rules.fa', test=test, train=train,
        ratios=ratios, merge=True, ths=ths, mfs=mfs, nw=nw)

    ratios = np.arange(.14, 0.32, .02)
    reduce_eval('automata/spyware-put.rules.fa', test=test, train=train,
        ratios=ratios, nw=nw)
    reduce_eval('automata/spyware-put.rules.fa', test=test, train=train,
        ratios=ratios, merge=True,nw=nw)

    ratios = np.arange(.02, 0.2, .02)
    reduce_eval('automata/imap.rules.fa', test=test, train=train,
        ratios=ratios, nw=nw)
    reduce_eval('automata/imap.rules.fa', test=test, train=train,
        ratios=ratios, merge=True,nw=nw)

    '''
    ratios = np.arange(.02, 0.22, .02)
    reduce_eval('automata/web-activex.rules.fa', test=test, train=train,
        ratios=ratios, nw=nw)
    reduce_eval('automata/web-activex.rules.fa', test=test, train=train,
        ratios=ratios, merge=True,nw=nw)
    '''

    ratios = np.arange(.26, 0.5, .03)
    reduce_eval('automata/l7-all.fa', test=test, train=train,
        ratios=ratios, nw=nw)
    reduce_eval('automata/l7-all.fa', test=test, train=train,
        ratios=ratios, merge=True,nw=nw)

def main():
    reduce_many()

if __name__ == '__main__':
    main()
