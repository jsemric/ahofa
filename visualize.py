#!/usr/bin/env python3

import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np
import pandas as pd
import sys
import re
import os

def err_dist(df1,df2):
    df1 = df1.set_index('automaton')
    df1['ce'] = df1.cfp / df1.total
    df2 = df2.set_index('automaton')
    df1.drop('pcap',axis=1,inplace=True)
    df = pd.concat([df2, df1],axis=1, join='inner')
    df = df.reset_index()
    df = df.loc[(df.automaton.str.startswith('sprobe')) & \
        (df.method == 'prune')]
    #sns.violinplot(x='ratio',y='ce',data=df,ineer=None, color='#dadee5',cut=0)
    sns.stripplot(x='ratio',y='ce',data=df,size=4,jitter=1)
    plt.title('CE distribution of sprobe pruning reduction')
    plt.ylabel('CE')
    plt.xlabel('reduction ratio')
    plt.grid()
    plt.show()

def to_latex(df):
    # 1.1 A vs C - 2 graphs + rabit line
    print('#1 prune ce vs ae')
    print(df.loc[(df.automaton == 'sprobe') & (df.method == 'prune'),
        ['ratio','ae','ce','ap','cp']].set_index('ratio').to_latex())

    print('#4 large nfa')
    for i in df.automaton.unique():
        print(i)
        #d = df.loc[(df.method != 'bfs') & (df.automaton == i)]
        d = df.loc[df.automaton == i]
        d = d.pivot_table(index='ratio',columns='method',values=['ce','cp'])
        print(d.to_latex())

def make_plot(df, nfa_name, *, var='ce', xmin = 0, ymax=None, drop=None,
    save=None):

    cols = ['method','ratio','ce','ae','cp','ap']
    sprobe = df.loc[df.automaton == nfa_name, cols]
    sprobe = sprobe.loc[sprobe.ratio > xmin]
    fig, ax = plt.subplots()

    markers = {'prune':'X','bfs':'s','merge':'o'}
    colors = {'prune':'blue','bfs':'gold','merge':'firebrick'}

    for k, g in sprobe.loc[sprobe['method'] != drop].groupby('method'):
        ax = g.plot(ax=ax, kind='line', x='ratio', y=var, label=k, c=colors[k],
            marker=markers[k], markersize=8, linestyle='--')

    ax.set_title(nfa_name + ' ' + var.upper())
    ax.set_ylabel(var.upper())
    if ymax:
        ax.set_ylim((-0.005,ymax))
    ax.set_xlabel('reduction ratio')
    plt.grid()
    plt.legend(loc='best')
    if save:
        ax.get_figure().savefig('/home/james/Desktop/bt/figures/{}.png'.format(
            save))
    else:
        plt.show()

def main():
    df1 = pd.read_csv('experiments/eval.csv').drop_duplicates()

    df2 = pd.read_csv('experiments/reduction.csv').drop_duplicates()
    df2['method'] = 'prune'
    df2.loc[df2.pcap == 'None', 'method'] = 'bfs'
    df2.loc[df2.th.notnull(), 'method'] = 'merge'
    #err_dist(df1,df2)

    df1 = df1.groupby('automaton').sum()
    df2 = df2.set_index('automaton')
    df = pd.concat([df1, df2], axis=1, join='inner').reset_index()

    df['automaton'] = df.automaton.str.replace('\.\d.*','')
    df['ce'] = df.cfp / df.total
    df['ae'] = df.afp / df.total
    df['cp'] = df.ctp / (df.ctp + df.cfp)
    df['ap'] = df.atp / (df.atp + df.afp)

    df = df.sort_values('ratio')
    for i in df:
        if df[i].dtype == np.float64: df[i] = round(df[i], 4)

    merge_par_test = df.loc[(df.th.notnull()) & (df['th'] != .995)]
    df = df.loc[(df.th.isnull()) | (df['th'] == .995)]

    to_latex(df)
    #make_plot(df, 'sprobe', xmin=.19, var='ce')
    # 1.2 prune vs bfs - done
    '''
    make_plot(df, 'sprobe', xmin=.19, var='ce', drop='merge',
        save='sprobe-ce-prune')
    make_plot(df, 'sprobe', xmin=.19, var='cp', drop='merge',
        save='sprobe-cp-prune')

    # 2.1 merging - done
    make_plot(df, 'sprobe', xmin=.19, var='ce', drop='bfs',
        save='sprobe-ce-merge')
    make_plot(df, 'sprobe', xmin=.19, var='cp', drop='bfs',
        save='sprobe-cp-merge')
    #'''

    # 2.2 different pars backdoor
    '''
    d = merge_par_test[['th','fm','ce']]
    d['th'] = round(d['th'],2)
    d['fm'] = round(d['fm'],2)
    data = d.pivot_table(index='th',columns='fm',values='ce')
    sns.heatmap(data, annot=True, fmt='g', cmap='viridis')
    plt.xlabel('maximal frequency')
    plt.ylabel('threshold')
    plt.title('CE of merging with different parameters')
    plt.show()
    #'''

    # 4 large nfas
    '''
    make_plot(df, 'backdoor.rules', ymax=.08, var='ce', save='backdoor-ce')
    make_plot(df, 'backdoor.rules', var='cp', save='backdoor-cp')
    # spyware
    make_plot(df, 'spyware-put.rules', var='ce', save='spyware-ce')
    make_plot(df, 'spyware-put.rules', var='cp', save='spyware-cp')
    # imap
    make_plot(df, 'imap.rules', var='ce', save='imap-ce')
    make_plot(df, 'imap.rules', var='cp', save='imap-cp')
    # l7
    make_plot(df, 'l7-all', var='ce', save='l7-all-ce')
    make_plot(df, 'l7-all', var='cp', save='l7-all-cp')
    #'''

if __name__ == "__main__":
    main()
