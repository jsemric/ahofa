#!/usr/bin/env python3

import sys
from nfa import Nfa

def main():
    if len(sys.argv) < 3:
        raise SystemError('two arguments required: INPUT OUTPUT')
    a = Nfa.parse(sys.argv[1])
    a.merge_redundant_states()
    with open(sys.argv[2], 'w') as f: a.print(f)

if __name__ == '__main__':
    main()
