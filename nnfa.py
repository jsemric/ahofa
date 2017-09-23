#!/usr/bin/env python3

from collections import defaultdict

class NetworkNfa:

    def __init__(self):
        self._transitions = list(defaultdict(set))
        self._initial_state = None
        self._final_states = set()

    @property
    def state_count(self):
        return len(self._transitions)

    @property
    def pred(self):
        pred = [set() for i in range(self.state_count)]

        for state, rules in enumerate(self._transitions):
            for key, value in rules.items():
                for q in value:
                    pred[q].add(state)

        return pred

    @property
    def succ(self):
        succ = [set() for i in range(self.state_count)]

        for state, rules in enumerate(self._transitions):
            for key, value in rules.items():
                for q in value:
                    succ[state].add(q)

        return succ

    @property
    def state_depth(self):
        succ = self.succ
        sdepth = [0 for i in range(self.state_count)]
        actual = set([self._initial_state])
        empty = set()
        depth = 0
        while True:
            empty = empty.union(actual)
            new = set()
            for q in actual:
                sdepth[q] = depth
                new = new.union(succ[q])
            new -= empty
            actual = new
            if not new:
                break
            depth += 1
        return sdepth

    def _add_state(self):
        self._transitions.append(defaultdict(set))
        return self.state_count

    def _add_rule(self, pstate, qstate, symbol):
        if 0 <= symbol <= 255 and pstate < self.state_count and \
        qstate < self.state_count:
            self._transitions[pstate][symbol].add(qstate)
        else:
            raise RuntimeError('Invalid rule format')

    def _add_initial_state(self, state):
        if state < self.state_count:
            self._initial_state = state
        else:
            raise RuntimeError('Invalid rule format')

    def _add_final_state(self, state):
        if state < self.state_count:
            self._final_states.add(state)
        else:
            raise RuntimeError('Invalid final state')

    def _build(self, init, transitions, finals):
        self._initial_state = init
        self._final_states = finals
        self._transitions = transitions

    def write_fa(self):
        yield self._initial_state
        for state, rules in enumerate(self._transitions):
            for key, value in rules.items():
                for q in value:
                    yield ' '.join((str(state),str(q),hex(key)))
        yield from self._final_states

    def print_fa(self):
        for line in self.write_fa():
            print(line)

    def _has_path_over_alph(self, state1, state2):
        alph = [1 for x in range(256)]
        for key,val in self._transitions[state1].items():
            if state2 in val:
                alph[key] = 0
            else:
                return False

        return not sum(alph)

    def remove_same_states(self):
        '''
        TODO Add comment.
        '''
        tmp = set()
        succ = self.succ
        pred = self.pred
        for s in self.succ[self._initial_state]:
            if self._has_path_over_alph(self._initial_state,s) and \
               self._has_path_over_alph(s,s):
                tmp.add(s)

        if tmp:
            state = tmp.pop()
        # remove states & add transitions
        for s in tmp:
            for p in pred[s]:
                for key,val in self._transitions[p].items():
                    val.discard(s)

            for key,val in self._transitions[s].items():
                for x in val:
                    self._add_rule(state,x,key)

        return self.remove_unreachable()

    def add_selfloop(self, state):
        for c in range(256):
            self._transitions[state][c] = set([state])

    def selfloop_to_finals(self):
        for x in self._final_states:
            self.add_selfloop(x)

    def remove_unreachable(self):
        succ = self.succ
        actual = set([self._initial_state])
        reached = set()
        while True:
            reached = reached.union(actual)
            new = set()
            for q in actual:
                new = new.union(succ[q])
            new -= reached
            actual = new
            if not new:
                break

        state_map = dict()
        cnt = 0
        for x in reached:
            state_map[x] = cnt
            cnt += 1

        transitions = [defaultdict(set) for x in range(cnt)]
        for state, rules in enumerate(self._transitions):
            if state in state_map:
                for key, val in rules.items():
                    transitions[state_map[state]][key] = set([state_map[x] \
                    for x in val if x in state_map])

        initial_state = state_map[self._initial_state]
        final_states = [state_map[x] for x in self._final_states \
                         if x in state_map]
        self._build(initial_state, transitions, final_states)
