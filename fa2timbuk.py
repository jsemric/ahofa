#!/usr/bin/env python3

import sys
import re
import argparse


from nfa import Nfa

def fa2timbuk(fname=sys.argv[1]):
    nfa = Nfa.parse(fname)
    print('Automaton A @LFA')
    print('Ops x:0 ' + ' '.join(str(i)+':1' for i in range(256)))
    print('States ' + ' '.join('q'+str(x) for x in nfa.states))
    print('Final States ' + ' '.join('q'+str(x) for x in nfa._final_states))
    print('Transitions')
    print('x->q' + str(nfa._initial_state))

    for state, rules in nfa._transitions.items():
        for sym, states in rules.items():
            for q in states:
                print('{}(q{}) -> q{}'.format(sym,state,q))

def retain_numbers(string):
    return re.sub('[^0-9]', '', string)

def timbuk2fa(fname=sys.argv[1]):
    with open(fname,'r') as f:
        ts = 0
        for line in f:
            line = line[:-1]
            if line.startswith("Final States"):
                finals = [retain_numbers(x) for x in line.split()][2:]

            if ts == 1:
                init = line.split()[-1]
                print(retain_numbers(init))
                ts = 2
            elif ts == 2:
                a, p, q = re.sub('["\(\)\->]',' ',line).split()
                print(retain_numbers(p),retain_numbers(q),'0x'+a)

            if line.startswith("Transitions"):
                ts = 1

        for x in finals: print(x)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('input', metavar='NFA', type=str)
    parser.add_argument('-r','--reverse', action='store_true',
        help='convert timbuk to fa')
    args = parser.parse_args()
    if args.reverse:
        timbuk2fa(args.input)
    else:
        fa2timbuk(args.input)
        

main()
