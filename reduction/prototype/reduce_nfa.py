#!/usr/bin/env python3

from collections import defaultdict
import os, sys
import operator
import argparse
import random
import subprocess
import re

import nfa

global_counter = 0

def show_progress():
    global global_counter
    global_counter += 1
    if global_counter % 16 == 15:
        sys.stdout.write('#')
        sys.stdout.flush()
    if global_counter % 80 == 79:
        sys.stdout.write('\n')
        sys.stdout.flush()

def reduction_result(previous_states, current_states):
    sys.stderr.write(
        '{}/{} {:0.2f}%\n'.format(
            current_states, previous_states,
            current_states * 100 / previous_states)
        )

def merge_random(aut, pct=0, prefix=0, suffix=0):
    # result is mapping state->state
    res = {}
    # set of states which will be collapsed
    state_depth = aut.state_depth
    # FIXME not max depth
    max_depth = max(state_depth.values()) - suffix
    states = [
        state for state in aut.states
        if state_depth[state] > prefix and state_depth[state] < max_depth ]

    # create equivalence classes
    equiv_groups_count = int(aut.state_count * pct) - (aut.state_count - \
    len(states))
    print(equiv_groups_count, pct, aut.state_count, len(states))
    if not equiv_groups_count > 1:
        # minimal number of equivalence groups
        equiv_groups = [set() for x in range(3)]
    else:
        equiv_groups = [set() for x in range(equiv_groups_count)]

    for state in states:
        x = random.randrange(0, equiv_groups_count)
        equiv_groups[x].add(state)

    for eq in equiv_groups:
        if len(eq) > 1:
            p = eq.pop()
            res[p] = p
            for q in eq:
                res[q] = p
                aut.merge_states(p, q)
                show_progress()
        elif len(eq) == 1:
            p = eq.pop()
            res[p] = p

    aut.clear_final_state_transitions()
    aut.remove_unreachable()
    sys.stdout.write('\n')
    return res

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

def prune_freq(aut, freq, pct=0):
    marked = set()
    previous_state_count = aut.state_count
    if pct == 0:
        marked = set([s for s in aut.states if freq[s] == 0])
    else:
        # states sorted according to its frequency
        srt_states = sorted(list(aut.states), key=lambda x : freq[x])
        # number of removed states
        cnt = aut.state_count - int(aut.state_count * pct)
        for state in srt_states:
            if cnt:
                cnt -= 1
                marked.add(state)
            else:
                break

    aut.prune(marked)
    reduction_result(previous_state_count, aut.state_count)

def prune_bfs(aut, depth, ratio=0):
    previous_state_count = aut.state_count
    state_depth = aut.state_depth
    aut.prune(
        set([
            state for state in aut.states if state_depth[state] > depth
            ]))

    reduction_result(previous_state_count, aut.state_count)

def prune_linear(aut, pct=0.1):
    marked = set()
    previous_state_count = aut.state_count
    # states sorted according to its depth
    depth = aut.state_depth
    srt_states = sorted(list(aut.states), key=lambda x : -depth[x])
    # number of removed states
    cnt = aut.state_count - int(aut.state_count * pct)
    for state in srt_states:
        if cnt:
            cnt -= 1
            marked.add(state)
        else:
            break

    aut.prune(marked)
    reduction_result(previous_state_count, aut.state_count)

def generate_output(aut, folder, filename, to_dot):
    dest = os.path.join(folder,filename + '.fa')
    # create file with reduced automaton
    sys.stderr.write('Saving NFA to ' + dest + '\n')
    with open(dest,'w') as f:
        aut.print_fa(f)

    if not to_dot:
        return

    # create dot
    dest = os.path.join(folder,filename + '.dot')
    sys.stderr.write('Saving dot to ' + dest + '\n')
    with open(dest,'w') as f:
        aut.print_dot(f)
    # create jpg
    jpgdest = os.path.join(folder,filename + '.jpg')
    sys.stderr.write('Saving jpg to ' + jpgdest + '\n')
    subprocess.call(' '.join(['dot -Tjpg', dest, '-o', jpgdest]).split())

def main():
    '''
    types of reduction:
        prune:
            <+> BFS
            <+> frequency
            <+> random
        merging:
            <+> random
    '''
    parser = argparse.ArgumentParser()
    # general arguments
    # common reduction arguments
    general_parser = argparse.ArgumentParser(add_help=False)
    general_parser.add_argument(
        'aut', type=str, metavar='FILE', help='input file with automaton')
    general_parser.add_argument(
        '-o','--output', type=str, metavar='FILE',
        help='output file, if not specified everything is printed to stdout')

    general_parser.add_argument(
        '-s','--add-sl', action='store_true', help='add self-loops to final \
        states of result the automaton')

    general_parser.add_argument(
        '-r','--reduction', type=float, metavar='PCT', default=0.5,
        help='ratio of reduction of states')

    general_parser.add_argument(
        '-g','--gen-out', action='store_true', help='generate output in fa')
    general_parser.add_argument(
        '-d','--to-dot', action='store_true',
        help='generate output in dot and jpg format')

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
        '--suffix', type=int, default=2,
        help='do not merge states within given length of suffix')

    args = parser.parse_args()
    if args.command == None:
        sys.stderr.write("Error: no arguments\n")
        sys.stderr.write("Use 'prune' or 'merge' commands for reduction\n")
        exit(1)

    # parse target NFA
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
            prune_freq(aut, freq, args.reduction)
        elif args.bfs:
            rtype = 'bfs'
            prune_bfs(aut, args.bfs)
        elif args.linear:
            rtype = 'linear'
            rpar = 'r{}'.format(args.reduction)
            prune_linear(aut, args.reduction)
    elif args.command == 'merge':
        rtype = 'rand'
        rpar = 'r{}p{}'.format(args.reduction, args.prefix)
        merge_random(aut, args.reduction, args.prefix, args.suffix)

    if args.add_sl:
        aut.selfloop_to_finals()

    # output result
    if args.gen_out:
        folder = os.path.dirname(args.aut)
        output = re.sub('\.fa$', '', os.path.basename(args.aut))
        if args.output:
            if not os.path.isdir(args.output):
                output = re.sub('\.fa$', '', os.path.basename(args.output))
            folder = os.path.dirname(args.output)

        output += '-' + args.command
        output += '-' + rtype
        output += '-' + rpar if rpar else ''

        generate_output(aut, folder, output, args.to_dot)
    else:
        if args.output:
            with open(args.output, 'w') as out:
                for line in aut.write_fa():
                    print(line, file=out, end='')
        else:
            for line in aut.write_fa():
                print(line, end='')

if __name__ == "__main__":
    main()
