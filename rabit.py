#!/usr/bin/env python3
# Jakub Semric 2018

import sys
import os
import argparse
import subprocess
import tempfile

from nfa import Nfa

def search_for_file(fname):
    for root, dirs, files in os.walk('.'):
        if fname in files:
            return os.path.join(root, fname)
    return None

def main():

    parser = argparse.ArgumentParser()
    parser.add_argument(
        '-o','--output', type=str, metavar='FILE', default="automaton.fa"
        help='output file')

    parser.add_argument('input', metavar='NFA', type=str)

    parser.add_argument(
        '-s','--sub', type=str, metaver='xNFA',
        help='check whether L(NFA) >= L(xNFA)')

    args = parser.parse_args()

    if args.sub:
        jarfile = search_for_file('RABIT.jar')
        if jarfile == None:
            sys.stderr.write(
                'Error: cannot find RABIT tool in this directory\n')
            sys.exit(1)
        aut1 = Nfa.parse(args.NFA1)
        aut2 = Nfa.parse(args.NFA2)
        aut1.selfloop_to_finals()
        aut2.selfloop_to_finals()
        aut1_ba = tempfile.NamedTemporaryFile()
        aut2_ba = tempfile.NamedTemporaryFile()
        aut1.print(open(aut1_ba.name,'w'), how='ba')
        aut2.print(open(aut2_ba.name,'w'), how='ba')

        proc = 'java -jar ' + jarfile + ' ' + aut1_ba.name + ' ' + \
        aut2_ba.name + ' -fast -finite'
        subprocess.call(proc.split())

    elif args.sub:
        jarfile = search_for_file('Reduce.jar')
        if jarfile == None:
            sys.stderr.write(
                'Error: cannot find Reduce tool in this directory\n')
            sys.exit(1)
        if not args.output:
            sys.stderr.write('Error: no output specified\n')
            exit(1)
        ba_file = tempfile.NamedTemporaryFile()
        reduce_file = tempfile.NamedTemporaryFile()

        aut = Nfa.parse(args.input, 'fa')
        mapping = aut.extend_final_states()
        write_output(ba_file.name, aut.write(how='ba'))

        proc = "java -jar " + jarfile + " " + ba_file.name + \
        " 10 -sat -finite -o " + reduce_file.name
        subprocess.call(proc.split())
        aut = Nfa.parse(reduce_file.name, 'ba')

        aut.retrieve_final_states(mapping)
        # rename states
        max_label = max(aut.states) + 1
        vals = set(mapping.values())
        for s in aut.states:
            if s in vals:
                mapping[s] = max_label
                max_label += 1

        aut.rename_states(mapping)
        with open(args.output, 'w') as f:
            for i in aut.write:
                f.write(i)
        
if __name__ == "__main__":
    main()