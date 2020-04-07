#!/usr/bin/env python3
# Approximate NFA reduction and error evaluation of the reduction NFA.

import sys
import tempfile
import argparse
import multiprocessing

from nfa import Nfa
from reduction_eval import reduce_nfa, armc
from helpers import export_frequency

def main():
    parser = argparse.ArgumentParser(description='Approximate NFA reduction.')
    parser.add_argument('-r','--ratio', metavar='N', type=float,
        default=.2, help='reduction ratio')
    parser.add_argument('input', type=str, help='NFA to reduce')
    parser.add_argument('-n','--nw', type=int,
        default=multiprocessing.cpu_count() - 1,
        help='number of workers to run in parallel')
    parser.add_argument('--test', nargs='+', type=str,  metavar='PCAP',
        help='test pcap files')
    parser.add_argument('--train', type=str, metavar='PCAP',
        help='train pcap file')
    parser.add_argument('-d', '--dest-dir', dest='dest_dir',
        help='Destination dir for frequency export.')

    group = parser.add_mutually_exclusive_group()
    group.add_argument('-m','--merge', action='store_true',
        help='merging reduction')
    group.add_argument('-a','--armc', action='store_true',
        help='merging reduction inspired by abstract regular model checking')

    parser.add_argument('-th','--thresh', type=float, metavar='N',
        help='threshold for merging', default=.995)
    parser.add_argument('-mf','--maxfr', type=float, default=.1,
         metavar='N', help='max frequency of a state allowed to be merged')
    parser.add_argument('-o','--output', type=str,default='output.fa')
    args = parser.parse_args()

    if (args.merge or args.armc) and not args.train:
        raise SystemError('--train option is required when merging')

    # get NFA
    aut = Nfa.parse(args.input)
    filename = args.input.split('/')[-1].split('.')[0]

    if args.armc:
        # merge using armc and prune
        aut, m = armc(aut, args.train, ratio=args.ratio, th=args.thresh,
            merge_empty=False)
        sys.stderr.write('states merged: ' + str(m) + '\n')
    else:
        sys.stderr.write('reduction ratio: ' + str(args.ratio) + '\n')
        freq = aut.get_freq(args.train)
        if args.dest_dir:
            export_frequency(freq, '{}{}.sig'.format(args.dest_dir, filename), args.train)
            exit()
        aut, m = reduce_nfa(aut, freq, ratio=args.ratio, merge=args.merge,
            th=args.thresh, mf=args.maxfr)
        if args.merge:
            sys.stderr.write('states merged: ' + str(m) + '\n')

    with open(args.output,'w') as f:
        sys.stderr.write('saved as ' + args.output + '\n')
        aut.print(f)

    if args.test:
        sys.stderr.write('evaluation reduction error\n')
        reduced = args.output
        r = Nfa.eval_accuracy(args.input, args.output, ' '.join(args.test),
            nw=args.nw)
        total, fp, tp = 0, 0, 0
        for b in r.split('\n'):
            if b != '':
                _, _, s1, _, _, s2, s3 = b.split(',')
                total += int(s1)
                fp += int(s2)
                tp += int(s3)

        print('error:', round(fp/total,4))
        if tp + fp > 0:
            print('precision:', round(tp/(fp+tp),4))

if __name__ == '__main__':
    main()
