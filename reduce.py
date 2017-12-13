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

def get_freq(fname):
    ret = {}
    with open(fname, 'r') as f:
        for line in f:
            line = line.split('#')[0]
            if line:
                state, freq, *_ = line.split()
                ret[int(state)] = int(freq)

    return ret


def generate_output(*, folder, filename, extension):
    # generating filename
    while True:
        # random identifier
        hsh = ''.join([str(x) for x in random.sample(range(0, 9), 5)])
        dest = '{}.{}{}'.format(filename, hsh, extension)
        dest = os.path.join(folder, dest)
        if not os.path.exists(dest):
            break

    return dest

def execute_batch(batch_file):
    snortdir= 'min-snort'
    nfadir = 'data/nfa'
    resdir = 'data/results'

    parser = argparse.ArgumentParser()
    # general
    parser.add_argument('-i', '--input', type=str, nargs='+', help='input NFA')
    parser.add_argument('-n', '--nworkers', type=int, help='number of cores',
        default=1)
    parser.add_argument('-p', '--pcaps', type=str, nargs='+', help='pcap files')
    # what to do
    parser.add_argument('--error', action='store_true', help='compute error')
    parser.add_argument('--reduce', action='store_true', help='reduce')
    # reduction parameters
    parser.add_argument('-t', '--reduction-type', choices=['prune','ga','armc'],
        help='reduction type', default='prune')
    parser.add_argument('-r', '--reduce-to', type=float, nargs='+',
        help='% states', default=[0.1, 0.12, 0.14, 0.16, 0.18, 0.2])
    parser.add_argument(
        '-l','--state-labels', type=str, help='labeled nfa states')

    # remove '#' comments and collect the arguments
    args = list()
    with open(batch_file, 'r') as f:
        for line in f:
            line = line.split('#')[0]
            if line:
                args += line.split()

    args = parser.parse_args(args)
    #print(args)

    nfa_filenames = args.input.copy()

    if args.reduce:
        nfa_filenames = list()
        if args.state_labels == None:
            sys.stderr.write('Error: state frequencies are not specified\n')
            exit(1)

        # generate output file name
        for j in args.input:
            core = os.path.basename(j).split('.')[0]
            for i in args.reduce_to:
                fname = generate_output(folder=nfadir, filename=core,
                    extension='.r' + str(i) + '.fa')
                nfa_filenames.append(fname)
                prog = [str(x) for x in ['./nfa_handler','reduce', j, '-t',
                    'prune', '-r', i, '-o', fname, args.state_labels]]
                sys.stderr.write(' '.join(prog) + '\n')
                # invoke program for reduction
                subprocess.call(prog)



    if args.error:
        # get pcap files
        samples = set()
        for i in args.pcaps:
            samples |= set(glob.glob(i))

        # find target nfas to each input nfa
        for i in nfa_filenames:
            core = os.path.basename(i).split('.')[0]
            target_str = os.path.join(snortdir, core)
            # find target NFA file name
            target_nfa = glob.glob(target_str + '*.fa')
            if len(target_nfa) == 0:
                sys.stderr.write('Error: cannot find "' + target_str +'*.fa"\n')
                continue
            # get first occurrence
            target_nfa = target_nfa[0]
            # generate output file name
            output = generate_output(folder=resdir, filename=core,
                extension='.json')

            prog = [str(x) for x in ['./nfa_handler', 'error', target_nfa, i,
                '-o', output,'-n', args.nworkers]] + list(samples)
            sys.stderr.write(' '.join(prog) + '\n')
            # invoke program for error computation
            subprocess.call(prog)

def write_output(fname, buf):
    if fname:
        with open(fname, 'w') as f:
            for i in buf:
                f.write(i)
    else:
        for i in buf:
            print(i, end='')

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
        'dot', help='draw NFA',
        parents = [general_parser, nfa_input_parser])
    dot_parser.add_argument('-f', '--freq', type=str, help='heat map')
    dot_parser.add_argument(
        '-t', '--trans', action='store_true',
        help='show transition labels')
    dot_parser.add_argument('-s', '--show', action='store_true', help='show result')

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
        write_output(args.output, gen)
    elif args.command == 'dot':
        freq = None
        if args.freq:
            freq = get_freq(args.freq)
        aut = nfa.Nfa.parse_fa(args.input)
        gen = aut.write_dot(args.trans, freq)
        fname = args.output if args.output else 'nfa.dot'
        write_output(fname, gen)
        if args.show:
            image = fname.split('.dot')[0] + '.jpg'
            prog = 'dot -Tjpg ' + fname + ' -o ' + image
            subprocess.call(prog.split())
            prog = 'xdg-open ' + image
            subprocess.call(prog.split())


if __name__ == "__main__":
    main()
