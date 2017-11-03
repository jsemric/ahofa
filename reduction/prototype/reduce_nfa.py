#!/usr/bin/env python3

from collections import defaultdict
import os, sys
import operator
import argparse
import random

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
    assert(equiv_groups_count > 1)
    if not equiv_groups_count > 1:
        return
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
    # TODO
    depth = aut.state_depth

    succ = aut.succ
    pred = aut.pred
    for state in aut.states:
        if len(succ[state]) == 1:
            for x in succ[state]:
                if freq[x] > freq[state] and len(pred[x]) == 1:
                    raise RuntimeError('invalid frequencies')

def prune_freq(aut, freq, pct=0):
    # TODO
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
    state_count = aut.state_count
    sys.stderr.write(
        '{}/{} {:0.2f}%\n'.format(
            state_count, previous_state_count,
            state_count * 100 / previous_state_count))

def prune_bfs(aut, depth, ratio=0):
    previous_state_count = aut.state_count
    state_depth = aut.state_depth
    aut.prune(
        set([
            state for state in aut.states if state_depth[state] > depth
            ]))

    state_count = aut.state_count
    sys.stderr.write(
        '{}/{} {:0.2f}%\n'.format(
            state_count, previous_state_count,
            state_count * 100 / previous_state_count))

def prune_random(aut, pct):
    # TODO
    pass

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
        '--random', action='store_true', help='prune randomly')
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

    # reduce
    if args.command == 'prune':
        if args.freq:
            freq = get_nfa_freq(args.freq)
            check_freq(aut, freq)
            prune_freq(aut, freq, args.reduction)
        elif args.bfs:
            prune_bfs(aut, args.bfs)
        elif args.random:
            prune_random(aut, args.reduction)
    elif args.command == 'merge':
        merge_random(aut, args.reduction, args.prefix, args.suffix)

    if args.add_sl:
        aut.selfloop_to_finals()

    # output result
    if args.output:
        with open(args.output, 'w') as out:
            for line in aut.write_fa():
                print(line, file=out)
    else:
        for line in aut.write_fa():
            print(line)

if __name__ == "__main__":
    main()
