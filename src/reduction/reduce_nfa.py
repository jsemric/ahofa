#!/usr/bin/env python3

from collections import defaultdict
import os, sys
import argparse
import subprocess
import re
import datetime
import glob

import nfa
import reductions

def reduction_result(previous_states, current_states):
    sys.stderr.write(
        '{}/{} {:0.2f}%\n'.format(
            current_states, previous_states,
            current_states * 100 / previous_states)
        )

def reduce(reduction_function, args):
    reduction_function(*args)

def get_nfa_freq(fname):
    freq = {}
    with open(fname, 'r') as f:
        for line in f:
            state, fr, *_ = line.split()
            freq[int(state)] = int(fr)

    return freq

def check_freq(aut, freq):
    depth = aut.state_depth

    succ = aut.succ
    pred = aut.pred
    for state in aut.states:
        if len(succ[state]) == 1:
            for x in succ[state]:
                if freq[x] > freq[state] and len(pred[x]) == 1:
                    raise RuntimeError('invalid frequencies')

def generate_output(aut, folder, filename, pars, to_dot, rename=False):
    now = datetime.datetime.now()
    basename = '{}-{}{}{}{}.fa'.format(
        filename, now.year % 100, now.month, now.day, pars)
    dest = os.path.join(folder, basename)

    if rename and os.path.exists(dest):
        idx = 0
        while True:
            basename = '{}-{}{}{}i{}{}.fa'.format(
                filename, now.year % 100, now.month, now.day, idx, pars)
            dest = os.path.join(folder, basename)
            if os.path.exists(dest):
                idx+=1
            else:
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

def execute(args):
    aut = nfa.Nfa.parse_fa(args.aut)
    # reduction parameter, used for generating output
    rtype = ''
    rpar = ''

    # reduce
    if args.command == 'prune':
        if args.freq:
            rtype = 'freq'
            rpar = 'r{}'.format(args.reduction)
            freq = get_nfa_freq(args.freq)
            check_freq(aut, freq)
            reductions.prune_freq(aut, freq, args.reduction)
        elif args.bfs:
            rtype = 'bfs' + str(args.bfs)
            reductions.prune_bfs(aut, args.bfs)
        elif args.linear:
            rtype = 'linear'
            rpar = 'r{}'.format(args.reduction)
            reductions.prune_linear(aut, args.reduction)
    elif args.command == 'merge':
        if args.one_line:
            rtype = 'oneline-p' + str(args.prefix)
            reductions.one_line_reduction(aut, args.prefix, args.suffix)
        else:
            rtype = 'rand'
            rpar = 'r{}p{}'.format(args.reduction, args.prefix)
            reductions.merge_random(aut, args.reduction, args.prefix, args.suffix)
    else:
        raise RuntimeError('invalid option')

    if args.add_sl:
        aut.selfloop_to_finals()
    # output result
    if args.output == None or os.path.isdir(args.output):
        folder = '.'
        output = re.sub('\.fa$', '', os.path.basename(args.aut))
        if args.output:
            folder = os.path.dirname(args.output)

        par = '-' + args.command
        par += '-' + rtype
        par += '-' + rpar if rpar else ''

        return generate_output(
            aut, folder, output, par, args.to_dot, args.rename)
    else:
        if args.output:
            with open(args.output, 'w') as out:
                for line in aut.write_fa():
                    out.write(line)
        return args.output

def main():
    '''
    types of reduction:
        prune:
            <+> BFS
            <+> frequency
        merging:
            <+> random
    '''
    parser = argparse.ArgumentParser()
    parser.add_argument('-b', '--batch', type=str, help='use batch file')
    # general arguments
    # common reduction arguments
    general_parser = argparse.ArgumentParser(add_help=False)
    general_parser.add_argument(
        'aut', type=str, metavar='FILE', help='input file with automaton')
    general_parser.add_argument(
        '-o','--output', type=str, metavar='FILE',
        help='output file or directory, if not specified output file is '
        'generated automatically.')

    general_parser.add_argument(
        '-s','--add-sl', action='store_true', help='add self-loops to final \
        states of result the automaton')

    general_parser.add_argument(
        '-r','--reduction', type=float, metavar='PCT', default=0.5,
        help='ratio of reduction of states')

    general_parser.add_argument(
        '-d','--to-dot', action='store_true',
        help='generate output in dot and jpg format')

    general_parser.add_argument(
        '-n','--rename', action='store_true',
        help='renames file if already exists')

    # 2 commands for reduction
    subparser = parser.add_subparsers(
        help='type of the reduction',
        dest='command')

    # state pruning
    prune_parser = subparser.add_parser(
        'prune', help='reduce NFA by states pruning',
        parents = [general_parser])
    prune_mxgroup = prune_parser.add_mutually_exclusive_group()
    prune_mxgroup.add_argument(
        '--freq', type=str, metavar='FILE',
        help='reduce NFA according to frequencies')
    prune_mxgroup.add_argument(
        '--linear', action='store_true', help='prune linearly')
    prune_mxgroup.add_argument(
        '--bfs', type=int, metavar='DEPTH', help='prune like BFS')

    # state merging
    merge_parser = subparser.add_parser(
        'merge', help='reduce NFA by states merging',
        parents = [general_parser])
    merge_parser.add_argument(
        '--prefix', type=int, default=2,
        help='do not merge states within given length of prefix')
    merge_parser.add_argument(
        '--suffix', type=int, default=0,
        help='do not merge states within given length of suffix')
    merge_mxgroup = merge_parser.add_mutually_exclusive_group()
    merge_mxgroup.add_argument(
        '--rand', action='store_true',
        help='merge randomly, this is set by default')
    merge_mxgroup.add_argument(
        '--one-line', action='store_true',
        help='merge states on the same depth level')

    args = parser.parse_args()
    if args.command == None and args.batch == None:
        sys.stderr.write("Error: no arguments\n")
        sys.stderr.write("Use 'prune' or 'merge' commands for reduction\n")
        exit(1)

    # reduce
    if args.command != None:
        execute(parser.parse_args())
    else:
        # batch file execution variables
        binput = str()
        boutdir = '.'
        eoutdir = 'data/errors/'
        workers = 1
        pcaps = None
        rename = None
        with open(args.batch, 'r') as bf:
            buf = str()
            for line in bf:
                # remove hash comments
                line = line.split('#')[0]
                if line == '':
                    continue
                if line.startswith('input'):
                    binput = line.split('=')[1].strip()
                elif line.startswith('outdir'):
                    boutdir = line.split('=')[1].strip() + '/'
                elif line.startswith('pcaps'):
                    pcaps = line.split('=')[1].strip()
                elif line.startswith('workers'):
                    workers = line.split('=')[1].strip()
                elif line.startswith('rename=yes'):
                    rename = True
                else:
                    buf += line

        if not os.path.exists(boutdir):
            os.makedirs(boutdir)
        if not os.path.exists(eoutdir):
            os.makedirs(eoutdir)

        res = []
        for line in buf.split('\n'):
            try:
                if line:
                    exe = line.split() + ['-o', str(boutdir), str(binput)]
                    if rename:
                        exe.append('--rename')

                    sys.stderr.write(' '.join(exe) + '\n')
                    res.append(execute(parser.parse_args(exe)))
            except Exception as e:
                sys.stderr.write('Error: ' + str(e))

        if pcaps:
            samples = set()
            for i in pcaps.split():
                samples |= set(glob.glob(i))
            sys.stderr.write('Samples:\n' + '\n'.join(samples) + '\n\n')
            # compute error
            for i in res:
                sys.stderr.write('Computing error for {}\n'.format(i))
                output = re.sub('\.fa$', '.json', os.path.basename(i))
                output = os.path.join(eoutdir, output)
                proc = ['./nfa_error', binput, i, '-j', '-o', output,
                    '-n',workers] + list(samples)
                subprocess.call(proc)

if __name__ == "__main__":
    main()
