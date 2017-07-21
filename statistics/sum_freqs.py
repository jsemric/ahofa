#!/usr/bin/env python3

import os
import argparse
from collections import defaultdict
import sys
import math
import re
import glob

class FreqReader:

    def __init__(self):
        self.body = str()
        self.state_freq = defaultdict(int)
        self.state_depth = dict()

    def add_freq(self, fname):
        with open(fname, 'r') as f:
            string = f.read()
            self.body, rest = re.split('=+\n', string)
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
                yield ' '.join([str(key),str(val),str(self.state_depth[key])]) \
                      + '\n'
        else:
            for key,val in self.state_freq.items():
                yield str(key) + ' ' + str(val) + '\n'

    def freq2log(self):
        self.state_freq = { key:math.log(val) if val != 0 else 0 \
                           for key,val in self.state_freq.copy().items() }

def main():

    parser = argparse.ArgumentParser()
    parser.add_argument('-i','--input', type=str, metavar='FILE', nargs='+', \
                        help='input file with automaton')
    parser.add_argument('-o','--output', type=str, metavar='FILE', \
                        help='output file with automaton')
    parser.add_argument('-d','--print-depths', action='store_true', \
                        default=False, help='make gnuplot output')
    parser.add_argument('-b','--print-body', action='store_true', \
                        default=False, help='print also body of NFA')
    parser.add_argument('-l','--log', action='store_true', \
                        default=False, help='use logarithm')

    args = parser.parse_args()

    if args.input is None:
        sys.stderr.write('Error: no input file specified\n')
        sys.exit(1)

    fr = FreqReader()
    for f in args.input:
        for i in glob.glob(f):
            fr.add_freq(i)

    if args.log:
        fr.freq2log()

    if args.output is None:
        for line in fr.write(args.print_body, args.print_depths):
            print(line,end='')
    else:
        with open(args.output, 'w') as f:
            for line in fr.write(args.print_body, args.print_depths):
                f.write(line)


if __name__ == "__main__":
    main()
