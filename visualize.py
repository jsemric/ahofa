#!/usr/bin/env python3

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

def plot_errors(fname):
    df = pd.read_csv(fname)
    # aggregate all values for each reduction
    # plot scatter plot

def main():
    plot_errors('sprobe.csv')

def pcap_analysis(fname='mc.txt'):
    # read with mixed data type, numpy stores result in 1d structured array
    data = np.log(np.loadtxt(open(fname,'r'), delimiter=' '))
    plt.imshow(data, cmap='hot', interpolation='nearest')
    plt.show()

if __name__ == "__main__":
    main()
