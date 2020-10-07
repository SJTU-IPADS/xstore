"""
Because scan is **much** slower than lookups,
we use fewer threads per client to plot the graph. 
Note! the corotuine should be fixed at 6, 
otherwise the latency will mismatch the one in lookup test
client used: (machines, threads)
[1,8], [2,8],[3,12],[4,12], ...
"""

rpc_thpts = [4.94222693, 7.37954560, 10.42902354,
             10.79378237, 10.73372636, 10.73469128]
rpc_lats = [9.38079, 13.3688, 19.0808, 25.8925, 32.3814, 35.6856]

sc_thpts = [3.81797433, 7.13403392, 7.92998338,
            7.9310981, 7.93144444, 7.92925517]
sc_lats = [12.1694, 12.7458, 21.5496, 32.0357, 40.168, 48.1636]
