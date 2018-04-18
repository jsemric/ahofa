#!/usr/bin/env python3

import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np
import pandas as pd
import sys
import re
import os

def error_dist(df1,df2):
    df1 = df1.set_index('reduced')
    df = pd.concat([df2, df1],axis=1, join='inner')
    df = df[df['iter'] == 1]
    df['accuracy'] = (1 - df['fp_c'] / df['total'])
    df['precision'] = (df['pp_c'] / (df['pp_c'] + df['fp_c']))
    df = df[['accuracy','ratio','precision']]
    df = df.reset_index()
    df['reduced'] = df['reduced'].apply(lambda x: re.sub('\..*$','',x))
    df = df.set_index('reduced')

    for i in set(df.index):
        #sns.boxplot(x='ratio',y='accuracy',data=df.loc[i])
        sns.violinplot(x='ratio',y='accuracy',data=df.loc[i],ineer=None,
            color='gray')
        sns.stripplot(x='ratio',y='accuracy',data=df.loc[i],size=2,jitter=1)
        plt.title(i)
        plt.show()

def display(df, rowname, *, save=False):
    df_nfa =  df.loc[rowname].unstack(level=[1])
    diff = abs((df_nfa['accuracy',0] - df_nfa['accuracy',1]).mean())

    if diff < 0.005:
        # display only pruning
        df_nfa = df_nfa.loc[:, (slice(None),0)]
        df_nfa.columns = ['accuracy','precision']
        if df_nfa.loc[:, 'precision'].sum() == 0:
            df_nfa = df_nfa.drop('precision',axis=1)
        ax = df_nfa.plot(title=rowname, marker='o')
        ax.margins(0.05, 0.05)
        ax.grid()
        if save:
            ax.get_figure().savefig('figures/{}.png'.format(rowname))
        else:
            plt.show()
    else:
        # display all iterations
        ax = df_nfa['accuracy'].plot(title=rowname, marker='o')
        ax.set_ylabel('accuracy')
        ax.margins(0.05, 0.05)
        ax.grid()
        if save:
            ax.get_figure().savefig('figures/{}-accuracy.png'.format(rowname))
        else:
            plt.show()

        ax = df_nfa['precision'].plot(title=rowname, marker='o')
        ax.set_ylabel('precision')
        ax.margins(0.05, 0.05)
        ax.grid()
        if save:
            ax.get_figure().savefig('figures/{}-precision.png'.format(rowname))
        else:
            plt.show()

def all_iterations(df):
    df = df.set_index(['reduced','ratio','iter'])
    style = 'fast'
    plt.style.use(style)
    toplevel = set([x for x, *_ in list(df.index)])

    for i in toplevel:
        display(df, i, save=False)

def merge_vs_prune(df):
    # just 1-step merge at max
    df = df[df['iter'] < 2]
    df['type'] = df['iter'].map({1:'merge',0:'prune'}).astype('category')
    df = df.drop(columns=['iter'])
    df = df.set_index(['reduced','ratio','type'])

    style = 'fast'
    plt.style.use(style)
    toplevel = set([x for x, *_ in list(df.index)])
    for rowname in toplevel:
        df_nfa =  df.loc[rowname].unstack(level=[1])
        diff = abs(
            (df_nfa['accuracy','merge'] - df_nfa['accuracy','prune']).mean())

        if diff < 0.005:
            # display only pruning
            df_nfa = df_nfa.loc[:, (slice(None),'prune')]
            df_nfa.columns = ['accuracy','precision']
            if df_nfa.loc[:, 'precision'].sum() == 0:
                df_nfa = df_nfa.drop('precision',axis=1)
            ax = df_nfa.plot(title=rowname, marker='o')
            ax.margins(0.05, 0.05)
            ax.grid()
            plt.show()
        else:
            # display all iterations
            ax = df_nfa['accuracy'].plot(title=rowname, marker='o')
            ax.set_ylabel('accuracy')
            ax.margins(0.05, 0.05)
            ax.grid()
            plt.show()
            ax = df_nfa['precision'].plot(title=rowname, marker='o')
            ax.set_ylabel('precision')
            ax.margins(0.05, 0.05)
            ax.grid()
            plt.show()

def main():
    error = 'experiments/error.csv'
    reduction = 'experiments/reduction.csv'

    df1 = pd.read_csv(error).drop_duplicates()
    df2 = pd.read_csv(reduction, index_col='reduced').drop_duplicates()

    error_dist(df1,df2)
    dfe = df1.groupby('reduced').sum()

    dfe['accuracy'] = (1 - dfe['fp_c'] / dfe['total'])
    dfe['precision'] = (dfe['pp_c'] / (dfe['pp_c'] + dfe['fp_c']))
    dfe = dfe[['accuracy', 'precision']]

    dfr = df2[['ratio','iter']]
    comb = pd.concat([dfr, dfe],axis=1, join='inner')
    comb = comb.reset_index()
    comb['reduced'] = comb['reduced'].apply(lambda x: re.sub('\..*$','',x))

    all_iterations(comb)
    merge_vs_prune(comb)

def pcap_analysis(fname='mc.txt'):
    # read with mixed data type, numpy stores result in 1d structured array
    data = np.log10(np.loadtxt(open(fname,'r'), delimiter=' '))
    close(filename)
    plt.imshow(data, cmap='jet', interpolation='nearest')
    plt.colorbar()
    plt.show()

if __name__ == "__main__":
    main()
