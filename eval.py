#!/usr/bin/env python3

import numpy as np

from reduce_eval import reduce_eval

def main():
    test = ['pcaps/geant.pcap*', 'pcaps/meter4-1*']
    train = ['pcaps/geant2.pcap']
    nw = 14
    
    ratios = np.arange(.14, 0.32, .02)
    reduce_eval('automata/sprobe.fa', test=test, ratios=ratios, nw=nw)
    reduce_eval('automata/sprobe.fa', test=test, train=train, ratios=ratios,
        nw=nw)
    reduce_eval('automata/sprobe.fa', test=test, train=train, ratios=ratios,
        merge=True, nw=nw)

    ratios = np.arange(.14, 0.32, .02)
    reduce_eval('automata/backdoor.rules.fa', test=test, ratios=ratios, nw=nw)
    reduce_eval('automata/backdoor.rules.fa', test=test, train=train,
        ratios=ratios, nw=nw)
    reduce_eval('automata/backdoor.rules.fa', test=test, train=train,
        ratios=ratios, merge=True,nw=nw)

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

    ratios = np.arange(.02, 0.22, .02)
    reduce_eval('automata/imap.rules.fa', test=test, train=train,
        ratios=ratios, nw=nw)
    reduce_eval('automata/imap.rules.fa', test=test, train=train,
        ratios=ratios, merge=True,nw=nw)

    ratios = np.arange(.02, 0.22, .02)
    reduce_eval('automata/web-activex.rules.fa', test=test, train=train,
        ratios=ratios, nw=nw)
    reduce_eval('automata/web-activex.rules.fa', test=test, train=train,
        ratios=ratios, merge=True,nw=nw)

    ratios = np.arange(.24, 0.5, .03)
    reduce_eval('automata/l7-all.fa', test=test, train=train,
        ratios=ratios, nw=nw)
    reduce_eval('automata/l7-all.fa', test=test, train=train,
        ratios=ratios, merge=True,nw=nw)

if __name__ == '__main__':
    main()