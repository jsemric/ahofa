#!/usr/bin/env python3
# Jakub Semric 2018

import argparse
import subprocess
import math

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
        '-o','--output', type=str, metavar='FILE', default="Aut.dot",
        help='output file')

    parser.add_argument('input', metavar='NFA', type=str)
    parser.add_argument('-f', '--freq', type=str, help='heat map')
    parser.add_argument(
        '-t', '--trans', action='store_true',
        help='show transition labels')
    parser.add_argument(
        '-r', '--rules', type=int, help='number of rules to show')
    parser.add_argument(
        '-d', '--depth', type=int, help='maximal depth of a state to display')

    args = parser.parse_args()

    aut = Nfa.parse(args.input)

    freq = None
    if args.freq:
        freq = get_freq(args.freq)

    states = set(aut.states)
    if args.rules:
        rules = list(aut.fin_pred().values())[0:args.rules]
        states = set([s for subl in rules for s in subl])

    if args.depth:
        depth = aut.state_depth
        states = set(filter(lambda x: depth[x] < args.depth, states))

    out = aut.write_dot(
        show_trans=args.trans, freq=freq, states=states,
        freq_scale=lambda x: math.log(x + 2), show_diff=0)

    write_output(args.output, out)
    image = args.output.split('.dot')[0] + '.jpg'
    prog = 'dot -Tjpg ' + args.output + ' -o ' + image
    subprocess.call(prog.split())
    prog = 'xdg-open ' + image
    subprocess.call(prog.split())

if __name__ == "__main__":
    main()
