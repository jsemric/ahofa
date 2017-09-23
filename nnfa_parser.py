#!/usr/bin/env python3

import re
import nnfa
from collections import defaultdict

class NetworkNfaParser:

    RE_transition_FA_format = re.compile('^\w+\s+\w+\s+\w+$')
    RE_state_FA_format = re.compile('^\w+$')

    def __init__(self):
        self._transitions = list(defaultdict(set))
        self._initial_state = None
        self._final_states = set()
        self._state_map = dict()

    @property
    def state_count(self):
        return len(self._state_map)

    def _add_state(self, state):
        if not state in self._state_map:
            self._state_map[state] = self.state_count
            self._transitions.append(defaultdict(set))

        return self._state_map[state]

    def _add_rule(self, pstate, qstate, symbol):
        pstate = self._add_state(pstate)
        qstate = self._add_state(qstate)
        symbol = int(symbol, 0)
        self._transitions[pstate][symbol].add(qstate)

    def _add_initial_state(self, state):
        state = self._add_state(state)
        self._initial_state = state

    def _add_final_state(self, fstate):
        if fstate in self._state_map:
            self._final_states.add(self._state_map[fstate])
        else:
            raise RuntimeError('invalid final states')

    def parse_fa(self, fname):
        rules = 0
        with open(fname, 'r') as f:
            for line in f:
                # erase new line at the end of the string
                if line[-1] == '\n':
                    line = line[:-1]

                if rules == 0:
                    # read initial state
                    if NetworkNfaParser.RE_state_FA_format.match(line):
                        self._add_initial_state(line)
                        rules = 1
                    else:
                        raise RuntimeError('invalid syntax: \"' + line + '\"')
                elif rules == 1:
                    # read transitions
                    if NetworkNfaParser.RE_transition_FA_format.match(line):
                        self._add_rule(*line.split())
                    elif NetworkNfaParser.RE_state_FA_format.match(line):
                        self._add_final_state(line)
                        rules = 2
                    else:
                        raise RuntimeError('invalid syntax: \"' + line + '\"')
                else:
                    if NetworkNfaParser.RE_state_FA_format.match(line):
                        self._add_final_state(line)
                    else:
                        raise RuntimeError('invalid syntax: \"' + line + '\"')

        out = nnfa.NetworkNfa()
        out._build(self._initial_state, self._transitions, self._final_states)

        return out
