#!/usr/bin/env python

#from ycsba_data import *

import numpy as np
import importlib
import matplotlib.pyplot as plt
from pylab import *

from sys import *


def main():

    module = "ycsba_data"
    if len(sys.argv) > 1:
        module = str(sys.argv[1])
    print("plot using module", module)

    res = importlib.import_module(module)

    fig, ax = plt.subplots()
    line1, = ax.plot(res.sc_thpts, res.sc_lats, "-",
                     marker='o', label='XStore')
    line2, = ax.plot(res.rpc_thpts, res.rpc_lats, "-", marker='x', label='RPC')
    try:
        # the module may not has the b_tree performance
        line3 = ax.plot(res.btree_thpts, res.btree_lats, "-",
                        marker='^', label='RDMA B+Tree')
    except:
        pass
    if module == "ycsbc_data":
        # plot addition line
        line3 = ax.plot(res.hybrid_thpts, res.hybrid_lats, "-",
                        marker='^', label='XStore dynamic')

    ax.legend(loc='center right')
    ax.set(xlabel="Thpt (Million reqs/sec)", ylabel="Latency (us)")
    ylim(0, 40)
    xlim(0, 15)
    ax.legend()
    plt.show()


if __name__ == "__main__":
    main()
