#!/usr/bin/env python

from gnuplot import *

"""
Experiment setup
24 threads, 1 server, 15 clients, 1 coroutine per client
`cmd: ./run.py  -st 24 -cc 1 -w ycsbc -t 24  -sa "-db_type ycsbh -ycsb_num 10000000" -ca='-total_accts=10000000 -need_hash -eval_type=sc -tree_depth=6' -e 60 -n 15 -c s -a "ycsb" -s val00`

learned.toml
`
[[stages]]
type = "lr"
parameter = 1

[[stages]]
type = "lr"
## typically, they use much larger models
#parameter = 400000
parameter = 1000000
`

"""
xstore = {
    1  : (3726453.39,6.3647),
    15 : (23050000.90,15.4801),
    156 : (28484344.79,58.1195), ## 15 nodes and 6 coroutines
    }

## server uses val15
xstore = {
    11 : (3615470.44,6.63858),
    21 : (7079234.86,6.63135),
    41 : (13501635.02,6.98426),
    51 : (16660321.30,7.15978),
    61 : (19607459.48,7.18007),
    62 : (26971380.10,9.7492),
    64 : (29106687.34,19.8292),
    66 : (29554755.31,29.1517),
    68 : (29374434.54,38.4844)
    }

rpc = {
    1: (5249564.33,4.22831),
    15 : (22167734.79,14.5627),
    156 : (23696299.77,85.9704)
    }

## new smaller clients
## client machines/coroutines
rpc = {
    11 : (5727938.46,4.05609),
    21 : (10646833.47,4.31306),
    41 : (20328136.24,5.84019),
    61 : (22664435.47,5.95521),
    62 : (25689277.26,11.7195),
    64 : (28326301.96,20.7278),
    66 : (27780373.12,31.6516),
    68: (27666390.58,42.7084),
#    612: (28701815.43,61.9478)
    }

btree = {
    1 : (1401612.70,17.1092),
    15 : (5898265.00,52.1502),
    156 : (7934677.16,242.546)
    }

btree = {
    11 : (1409538.18,17.5068),
    21 : (2634963.07,17.9625),
    41 : (4652759.33,20.6393),
    61 : (5873995.15,24.5293),
    62 : (6695124.44,44.1252),
    64 : (7659727.27,64.5163),
    66 : (7930168.56,109.375),
    68 : (7760601.14,152.057)
    }

ylim = 72

def main():
    # convert the results to gnuplot format
    output_res_2("ycsbc",xstore,rpc,btree)

if __name__ == "__main__":
    main()
