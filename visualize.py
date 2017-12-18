#!/usr/bin/env python

import matplotlib.pyplot as plt
import pandas as pd

nfa_data = pd.read_csv('data/sprobe.csv')
print(nfa_data.iloc[:,[x for x in range(6, 6 + 9)]])
exit(0)
nfa_data = pd.read_csv('data/results.csv')
nfa_geant_data = nfa_data[nfa_data['pcap'] != 'pcaps/week1.pcap']
plt.scatter(nfa_geant_data['reduction'], nfa_geant_data['ce'])
plt.show()
