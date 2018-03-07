#!/usr/bin/env python3
# Jakub Semric 2018

import sys
import os
import argparse
import subprocess
import tempfile
import re
import datetime
import glob
import random
import math
import numpy as np
from collections import defaultdict

from nfa import Nfa

def write_output(fname, buf):
    with open(fname, 'w') as f:
        for i in buf:
            f.write(i)

def get_freq(fname):
    ret = {}
    with open(fname, 'r') as f:
        for line in f:
            line = line.split('#')[0]
            if line:
                state, freq, *_ = line.split()
                ret[int(state)] = int(freq)

    return ret

def main():

    parser = argparse.ArgumentParser()
    parser.add_argument(
        '-o','--output', type=str, metavar='FILE', default="automaton.dot",
        help='output file')

    parser.add_argument('input', metavar='NFA', type=str)
    parser.add_argument('-f', '--freq', type=str, help='heat map')
    parser.add_argument(
        '-t', '--trans', action='store_true',
        help='show transition labels')
    parser.add_argument(
        '-r', '--rules', type=int, help='number of rules to show')

    args = parser.parse_args()

    # TODO
    aut = Nfa.parse(args.input)

    if args.freq:
        freq = get_freq(args.freq)
        
        gen = aut.write_dot(
            show_trans=args.trans, freq=freq,
            #states=set(s for s,f in _freq.items() if f > 10),
            #rules=10,
            freq_scale=lambda x: math.log(x + 2), show_diff=0)
    else:
        gen = aut.write_dot()

    write_output(args.output, gen)
    image = args.output.split('.dot')[0] + '.jpg'
    prog = 'dot -Tjpg ' + args.output + ' -o ' + image
    subprocess.call(prog.split())
    prog = 'xdg-open ' + image
    subprocess.call(prog.split())
        
if __name__ == "__main__":
    main()