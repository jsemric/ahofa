#!/usr/bin/env python3

import os
import argparse
from collections import defaultdict
import sys
import math

class FreqReader:

    def __init__(self):
        self.body = str()
        self.state_freq = defaultdict(int)
        self.state_depth = dict()

    def add_freq(self, fname):
        with open(fname, 'r') as f:
            string = f.read()
            self.body, rest = string.split('=====\n')
            for line in rest.split('\n'):
                if line:
                    s, f, d = line.split()
                    self.state_freq[s] += int(f)
                    self.state_depth[s] = int(d)

    def write(self, body=False, depth=False, log=False):
        if body:
            yield self.body
        if depth:
            for key,val in self.state_freq.items():
                if log and val != 0:
                    val = math.log(val)
                yield str(key) + ' ' + str(val) + ' ' + str(self.state_depth[key]) + '\n'
        else:
            for key,val in self.state_freq.items():
                if log and val != 0:
                    val = math.log(val)
                yield str(key) + ' ' + str(val) + '\n'

    def merge_same_depth(self):
        yield '# depth   frequency\n'
        inv_depth = defaultdict(set)
        for key,val in self.state_depth.items():
            inv_depth[val].add(key)

        for key,val in inv_depth.items():
            freq = 0
            cnt = 0
            for i in val:
                cnt += 1
                freq += sum([x for y,x in self.state_freq.items() if y == i])
            yield str(key) + ' ' + str(freq) + '\n'

def main():

    parser = argparse.ArgumentParser()
    parser.add_argument('-i','--input', type=str, metavar='FILE', nargs='+', \
                        help='input file with automaton')
    parser.add_argument('-o','--output', type=str, metavar='FILE', \
                        help='output file with automaton')
    parser.add_argument('-m','--merge-depths', action='store_true', \
                        default=False, help='make gnuplot output')
    parser.add_argument('-d','--print-depths', action='store_true', \
                        default=False, help='make gnuplot output')
    parser.add_argument('-p','--print-body', action='store_true', \
                        default=False, help='print also body of NFA')
    parser.add_argument('-l','--log', action='store_true', \
                        default=False, help='use logarithm')

    args = parser.parse_args()

    if args.input is None or args.output is None:
        sys.stderr.write('Error: no input/output file specified\n')
        sys.exit(1)

    fr = FreqReader()
    for f in args.input:
        fr.add_freq(f)

    if args.merge_depths:
        with open(args.output, 'w') as f:
            for line in fr.merge_same_depth():
                f.write(line)
    else:
        with open(args.output, 'w') as f:
            for line in fr.write(args.print_body, args.print_depths, args.log):
                f.write(line)


if __name__ == "__main__":
    main()
