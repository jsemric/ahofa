#!/usr/bin/env python3

import os
import argparse
import re
import sys
import subprocess
import tempfile

def ba_to_fa_format(input, output):
    with open(output,'w') as f1:
        with open(input,'r') as f2:
            for line in f2:
                if ',' in line:
                    a,s1,s2 = re.split('(?:,|->)\s*', line[:-1])
                    f1.write(s1[1:-1] + ' ' + s2[1:-1] + ' ' + a + '\n')
                else:
                    f1.write(line[1:-2] + '\n')

def fa_to_ba_format(input, output):
    with open(output,'w') as f1:
        with open(input,'r') as f2:
            for line in f2:
                if ' ' in line:
                    s1, s2, a = line.split()
                    f1.write(a + ',[' + s1 + ']' + '->[' + s2 + ']\n')
                else:
                    f1.write('[' + line[:-1] + ']\n')

def main():
    parser = argparse.ArgumentParser()
    mgroup = parser.add_mutually_exclusive_group()
    mgroup.add_argument('-i','--include', type=str, nargs=2, metavar='NFA')
    mgroup.add_argument('-m','--minimize', type=str, metavar='NFA')
    parser.add_argument('-o','--output', type=str, metavar='FILE',
    help='output file with automaton')

    args = parser.parse_args()

    if args.include:
        print(args.include[0] + ' <= ' + args.include[1])
        aut1_file = tempfile.NamedTemporaryFile()
        aut2_file = tempfile.NamedTemporaryFile()
        fa_to_ba_format(args.include[0], aut1_file.name)
        fa_to_ba_format(args.include[1], aut2_file.name)

        proc = 'java -jar RABIT.jar ' + aut1_file.name + ' ' \
        + aut2_file.name + ' -fast -finite'
        subprocess.call(proc.split())

    elif args.minimize:
        if not args.output:
            sys.stderr.write('Error: no output specified\n')
            exit(1)
        ba_file = tempfile.NamedTemporaryFile()
        reduce_file = tempfile.NamedTemporaryFile()
        fa_to_ba_format(args.minimize, ba_file.name)

        proc = "java -jar Reduce.jar " + ba_file.name + " 10 -sat -finite -o " \
        + reduce_file.name

        subprocess.call(proc.split())
        ba_to_fa_format(reduce_file.name, args.output)

    else:
        # XXX add some default action
        sys.stderr.write('Error: no options specified\n')
        sys.stderr.write("run with '-h' or '--help' for help\n")
        exit(1)

if __name__ == "__main__":
    main()
