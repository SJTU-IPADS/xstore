#!/usr/bin/env python

import numpy as np
import importlib
import matplotlib.pyplot as plt
from pylab import *

from common import *

from sys import *

def main():
    module = "ycsbc"

    if len(sys.argv) > 1:
        module = str(sys.argv[1])
    print("plot using module", module)

    res = importlib.import_module(module)

    plt.subplots_adjust(left=0.14,bottom=0.2, right=0.67, top=0.65)
    plt.tick_params(axis='both', which='major', labelsize=10)

    fig = plt.figure(num=1, figsize=(8, 5),frameon=False) ## 8,5: width, height
    ax = plt.gca()

    for i,d in enumerate(res.data):
        d = flat_latency_thpt(d)
        ax.plot(d[0],d[1],"-",marker="o",label=res.legends[i])

    plt.xlabel("Thpts req/sec")
    plt.ylabel("Latency (\u03BCs)")

    bottom, top = ylim()
    try:
        top = res.ylim
    except:
        pass
    ylim(0,top)

    ax.legend()

    foo_fig = plt.gcf() # 'get current figure'
    foo_fig.savefig(module + '.eps', format='eps', dpi=1000)

    plt.show()

if __name__ == "__main__":
    main()
