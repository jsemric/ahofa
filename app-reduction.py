#!/usr/bin/env python3

import sys, os
import tempfile
import argparse
import multiprocessing

from nfa import Nfa
from reduce_eval import reduce_nfa  

def main():
    #reduce_eval('automata/sprobe.fa', test=['pcaps/*k.pcap'],
    #    train='pcaps/10k.pcap', ratios=[.18,.2], merge=True,nw=3)

    parser = argparse.ArgumentParser()
    parser.add_argument('-r','--ratio', metavar='N', type=float,
        default=.2, help='reduction ratio')
    parser.add_argument('input', type=str, help='NFA to reduce')
    parser.add_argument('-n','--nw', type=int,
        default=multiprocessing.cpu_count() - 1,
        help='number of workers to run in parallel')
    parser.add_argument('--test', type=str, help='test pcap files')
    parser.add_argument('--train', type=str, help='train pcap file')
    parser.add_argument('-m','--merge', action='store_true',
        help='merging reduction')
    parser.add_argument('-th','--thresh', type=float,
        help='threshold for merging', default=.995)
    parser.add_argument('-mf','--maxfr', type=float, default=.1,
        help='max frequency of a state allowed to be merged')
    parser.add_argument('-o','--output', type=str,)
    args = parser.parse_args()

    if args.merge and not args.train:
        sys.stderr.write('Error: --train option is required when merging\n')
        exit(1)

    aut = Nfa.parse(args.input)
    freq = None

    aut, m = reduce_nfa(aut, aut.get_freq(args.train), args.ratio, args.merge,
        args.thresh, args.maxfr)
    sys.stderr.write('Merged: ' + str(m) + '\n')

    if args.output:
        with open(args.output,'w') as f:
            aut.print(f)
    else:
        aut.print()

    if args.test:
        if args.output:
            reduced = args.output
        else:
            reduced = tempfile.NamedTemporaryFile().name
        r = Nfa.eval_accuracy(args.input, reduced, args.test, nw=args.nw)
        _,_,total, _, _, fp, tp, _ = r.split(',')
        total, fp, tp = int(total), int(fp), int(tp)
        print('error:',fp/total)
        print('precision:',tp/(fp+tp))

if __name__ == '__main__':
    main()