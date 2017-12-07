#!/usr/bin/env python3
# automation of reduction, error computing and state labeling

import sys
import os
import argparse
import subprocess
import tempfile
import re
import datetime
import glob
import random

import aux_scripts.nfa as nfa

def search_for_file(fname):
    for root, dirs, files in os.walk('.'):
        if fname in files:
            return os.path.join(root, fname)
    return None

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

def nfa_to_ba(aut, output):
    with open(output, 'w') as f:
        for i in nfa.Nfa.fa_to_ba(aut.write_fa()):
            f.write(i)


def generate_output(aut, folder, filename, pars, to_dot):
    # generating filename
    while True:
        # random identifier
        hsh = ''.join([str(x) for x in random.sample(range(0, 9), 5)])
        basename = '{}-{}-{}'.format(filename, hsh, pars)
        dest = os.path.join(folder, basename + '.fa')
        if not os.path.exists(dest):
            break

    # create file with reduced automaton
    sys.stderr.write('Saving NFA to ' + dest + '\n')
    with open(dest, 'w') as f:
        aut.print_fa(f)

    if to_dot:
        # create dot
        dotdest = os.path.join(folder, basename + '.dot')
        sys.stderr.write('Saving dot to ' + dest_dot + '\n')
        with open(dest, 'w') as f:
            aut.print_dot(f)
        # create jpg
        jpgdest = os.path.join(folder, basename + '.jpg')
        sys.stderr.write('Saving jpg to ' + jpgdest + '\n')
        subprocess.call(' '.join(['dot -Tjpg', dest, '-o', jpgdest]).split())

    return dest

def execue_batch(batch_file):
    pass

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-b', '--batch', type=str, help='use batch file')

    # general arguments
    # common reduction arguments
    general_parser = argparse.ArgumentParser(add_help=False)
    general_parser.add_argument(
        '-o','--output', type=str, metavar='FILE',
        help='output file, if not specified output is printed to stdout')

    nfa_input_parser = argparse.ArgumentParser(add_help=False)
    nfa_input_parser.add_argument('input', metavar='NFA', type=str)

    # 4 commands for NFA
    subparser = parser.add_subparsers(
        help='command', dest='command')

    # rabit tool
    rabit_parser = subparser.add_parser(
        'issubset', help='if L(NFA1) is subset of L(NFA2)',
        parents = [general_parser])
    rabit_parser.add_argument('NFA1', type=str)
    rabit_parser.add_argument('NFA2', type=str)

    # reduce tool
    reduce_parser = subparser.add_parser(
        'min', help='minimizes NFA by bisimulation',
        parents = [general_parser, nfa_input_parser])

    # dot format and jpg format
    dot_parser = subparser.add_parser(
        'draw', help='draw NFA',
        parents = [general_parser, nfa_input_parser])

    # light-weight minimization
    lmin_parser = subparser.add_parser(
        'lmin', help='minimizes NFA by light-weight minimization',
        parents = [general_parser, nfa_input_parser])

    args = parser.parse_args()
    if args.command == None and args.batch == None:
        sys.stderr.write("Error: no arguments\n")
        sys.stderr.write("Use '--help' or '-h' for help.\n")
        exit(1)
    elif args.batch:
        sys.stderr.write('executing batch file\n')
        execute_batch(args.batch)
        exit(0)

    if args.command == 'min':
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
        fa_to_ba_format(args.input, ba_file.name)

        proc = "java -jar " + jarfile + " " + ba_file.name + \
        " 10 -sat -finite -o " + reduce_file.name

        subprocess.call(proc.split())
        ba_to_fa_format(reduce_file.name, args.output)
    elif args.command == 'issubset':
        jarfile = search_for_file('RABIT.jar')
        if jarfile == None:
            sys.stderr.write(
                'Error: cannot find RABIT tool in this directory\n')
            sys.exit(1)
        aut1 = nfa.Nfa.parse_fa(args.NFA1)
        aut2 = nfa.Nfa.parse_fa(args.NFA2)
        aut1.selfloop_to_finals()
        aut2.selfloop_to_finals()
        aut1_ba = tempfile.NamedTemporaryFile()
        aut2_ba = tempfile.NamedTemporaryFile()
        nfa_to_ba(aut1, aut1_ba.name)
        nfa_to_ba(aut2, aut2_ba.name)

        proc = 'java -jar ' + jarfile + ' ' + aut1_ba.name + ' ' + \
        aut2_ba.name + ' -fast -finite'
        subprocess.call(proc.split())
    elif args.command == 'lmin':
        aut = nfa.Nfa.parse_fa(args.input)
        aut.lightweight_minimization()
        gen = aut.write_fa()
    elif args.command == 'draw':
        aut = nfa.Nfa.parse_fa(args.input)
        gen = aut.write_dot()

    # write output
    if args.command == 'lmin' or args.command == 'draw':
        if args.output:
            with open(args.output, 'w') as f:
                for i in gen:
                    f.write(i)
        else:
            for i in gen:
                print(i, end='')

if __name__ == "__main__":
    main()