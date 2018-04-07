#!/usr/bin/env python3

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import sys
import re
import os

def barplots(df):
    # take the smallest mean among iterations and plot barplots with errors
    pass


def display(df, rowname, *, save=False):
    df_nfa =  df.loc[rowname].unstack(level=[1])
    diff = abs((df_nfa['ce',0] - df_nfa['ce',1]).mean())

    if diff < 0.005:
        # display only pruning
        df_nfa = df_nfa.loc[:, (slice(None),0)]
        df_nfa.columns = ['ce','ppr']
        if df_nfa.loc[:, 'ppr'].sum() == 0:
            df_nfa = df_nfa.drop('ppr',axis=1)
        ax = df_nfa.plot(title=rowname, marker='o')
        ax.margins(0.05, 0.05)
        if save:
            ax.get_figure().savefig('figures/{}.png'.format(rowname))
        else:
            plt.show()
    else:
        # display all iterations
        ax = df_nfa['ce'].plot(title=rowname, marker='o')
        ax.set_ylabel('classification error')
        ax.margins(0.05, 0.05)
        if save:
            ax.get_figure().savefig('figures/{}-ce.png'.format(rowname))
        else:
            plt.show()

        ax = df_nfa['ppr'].plot(title=rowname, marker='o')
        ax.set_ylabel('positive positive ratio')
        ax.margins(0.05, 0.05)
        if save:
            ax.get_figure().savefig('figures/{}-ppr.png'.format(rowname))
        else:
            plt.show()


def main():
    error = 'experiments/error2.csv'
    reduction = 'experiments/reduction2.csv'

    df1 = pd.read_csv(error).drop_duplicates()
    dfe = df1.groupby('reduced').sum()

    dfe['ce'] = (dfe['fp_c'] / dfe['total'])
    dfe['ppr'] = (dfe['pp_c'] / (dfe['pp_c'] + dfe['fp_c']))
    dfe = dfe[['ce', 'ppr']]

    df2 = pd.read_csv(reduction, index_col='reduced').drop_duplicates()
    dfr = df2[['ratio','iter']]
    comb = pd.concat([dfr, dfe],axis=1, join='inner')
    comb = comb.reset_index()
    comb['reduced'] = comb['reduced'].apply(lambda x: re.sub('\..*$','',x))
    comb = comb.set_index(['reduced','ratio','iter'])
    print(comb)

    plt.style.use('ggplot')
    toplevel = set([x for x, *_ in list(comb.index)])
    for i in toplevel:
        display(comb, i, save=False)

    exit(0)

def pcap_analysis(fname='mc.txt'):
    # read with mixed data type, numpy stores result in 1d structured array
    data = np.log10(np.loadtxt(open(fname,'r'), delimiter=' '))
    close(filename)
    plt.imshow(data, cmap='jet', interpolation='nearest')
    plt.colorbar()
    plt.show()

if __name__ == "__main__":
    main()
