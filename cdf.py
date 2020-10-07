#!/usr/bin/env python

import numpy as np
import importlib
import argparse

from pylab import *
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker

sample_gap = 100

def sample_show():
    # Create some test data
    dx = 0.01
    X  = np.arange(-2, 2, dx)
    Y  = exp(-X ** 2)

    # Normalize the data to a proper PDF
    Y /= (dx * Y).sum()

    # Compute the CDF
    CY = np.cumsum(Y * dx)

    # Plot both
    plot(X, Y)
    plot(X, CY, 'r--')

    show()

def plot_simple(X,Y):
    l = plot(X, Y)
    legend(handles=l)
    show()

def sample_data(data,gap):
    if ((len(data) / float(gap)) < 100):
        return data
    res = []
    for i in range(0,len(data),gap):
        res.append(data[i])
    return res

def plot_multi(l):
    lines = []

    # plot seperately
    for idx,i in enumerate(l):
        ax = plt.subplot(len(l) + 1,1,idx + 1)
#       limit = len(i.X) - 1
#        xlim(i.X[0],i.X[limit])
#        ylim(i.Y[0],i.Y[limit])
        #ax.xaxis.set_major_locator(ticker.MultipleLocator(sample_gap0))
        ll = plot(sample_data(i.X,sample_gap),sample_data(i.Y,sample_gap),".",label = i.title)
        xlabel(i.xlabel)
        ylabel(i.ylabel)
        legend(handles = ll)

    ## plot all in one
    """
    plt.subplot(len(l) + 1, 1, len(l) + 1)
    for i in l:
        x = sample_data(i.X,sample_gap)
        y = sample_data(i.Y,sample_gap)
        lines.append((plot(x,y,'.',label = i.title))[0])
    legend(handles = lines)
    """
    show()

def main():

    parser = argparse.ArgumentParser()
    parser.add_argument('-d', '--data',default="data",nargs="+")
    parser.add_argument('-s', '--sample',default=10000)

    args = parser.parse_args()

    global sample_gap
    sample_gap = int(args.sample)

    if type(args.data) is list:
        datas = [importlib.import_module(i) for i in args.data]
        plot_multi(datas)
    else:
        data = importlib.import_module(args.data)
        plot_simple(data.X,data.Y)

if __name__ == "__main__":
    main()
