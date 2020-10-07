#!/usr/bin/env python

from gnuplot import *
from common import *

"""
parameter = 400000
/run.py  -st 24 -cc 1 -w ycsba -t 24  -sa "-db_type dummy -ycsb_num 1000000" -ca='-total_accts=10000000 -need_hash -workloads=dynamic2' -e 120 -n 8 -c s -a "micro" -s val15
"""


xstore_points = [(1.15392e+06,0.0110791,0.177051,),(3.57179e+07,0.0122484,0.405161,),(3.12319e+07,0.0187675,0.663196,),(2.79234e+07,0.0217916,0.828355,),(3.00828e+07,0.0132346,0.705461,),(3.15646e+07,0.0220135,0.590522,),(2.99859e+07,0.0276641,0.688557,),(2.8283e+07,0.0344974,0.791608,),(2.95096e+07,0.025681,0.678845,),(3.09042e+07,0.0303514,0.580317,),(2.92514e+07,0.0338583,0.686201,),(2.78194e+07,0.0426033,0.768208,),(2.82989e+07,0.0265815,0.740534,),(3.1403e+07,0.0256092,0.504603,),(2.95874e+07,0.0402919,0.599113,),(2.84787e+07,0.0490037,0.679274,),(2.89587e+07,0.0399852,0.643352,),(3.06838e+07,0.034946,0.483786,),(2.98796e+07,0.0417055,0.53924,),(2.89411e+07,0.0459164,0.608611,),(2.80223e+07,0.0501009,0.670213,),(2.93587e+07,0.040765,0.540917,),(3.06902e+07,0.0408927,0.431041,),(2.96411e+07,0.0488854,0.497308,),(2.89085e+07,0.0547315,0.557771,),(2.80951e+07,0.0618542,0.610667,),(2.81678e+07,0.0476858,0.604394,),(3.03952e+07,0.0362087,0.385982,),(2.99449e+07,0.0429813,0.438549,),(2.90225e+07,0.0592867,0.493055,),(2.85084e+07,0.0661019,0.543523,),(2.7845e+07,0.0680134,0.588754,),(2.82933e+07,0.0524691,0.555235,),(2.97376e+07,0.0481291,0.374819,),(2.93338e+07,0.0515231,0.418022,),(2.88442e+07,0.0565707,0.464573,),(2.84958e+07,0.0628544,0.508471,),(2.78742e+07,0.0734452,0.548648,),(2.72348e+07,0.087072,0.586416,),(2.75239e+07,0.0748376,0.536686,),(2.91015e+07,0.0530701,0.352936,),(2.89656e+07,0.0547113,0.38114,),(2.88157e+07,0.0533222,0.421588,),(2.83865e+07,0.064603,0.459971,),(2.77465e+07,0.0818415,0.495707,),(2.72166e+07,0.0913219,0.5293,),(2.69463e+07,0.0935253,0.550246,),(2.67412e+07,0.090502,0.580101,),(2.63436e+07,0.0875916,0.596441,),(2.83762e+07,0.0568444,0.366291,),(2.82062e+07,0.0646272,0.366493,),(2.79764e+07,0.0694204,0.399769,),(2.78358e+07,0.0678703,0.432,),(2.74429e+07,0.0791944,0.452549,),(2.71626e+07,0.0867435,0.481772,),(2.67013e+07,0.0991882,0.50979,),(2.64481e+07,0.103924,0.536271,),(2.62849e+07,0.101292,0.561773,),(2.60287e+07,0.104498,0.561957,),(2.76768e+07,0.0622644,0.349213,),(2.75248e+07,0.0688968,0.350888,),(2.73464e+07,0.0780956,0.37947,),(2.72602e+07,0.0819645,0.407379,),(2.70185e+07,0.0881143,0.434415,),(2.67956e+07,0.0922785,0.451749,),(2.66098e+07,0.102194,0.468644,),(2.64152e+07,0.102496,0.492861,),(2.61836e+07,0.108349,0.516534,),(2.57161e+07,0.122354,0.538773,),(2.52673e+07,0.133477,0.567406,),(2.64606e+07,0.083525,0.420535,),(2.6702e+07,0.0790358,0.33615,),(2.66561e+07,0.0812365,0.352261,),(2.65843e+07,0.0847889,0.376831,),(2.64682e+07,0.0945345,0.40011,),(2.62165e+07,0.104418,0.422907,),(2.60737e+07,0.106157,0.444716,),(2.59646e+07,0.107819,0.458981,),(2.56522e+07,0.11983,0.480077,),(2.54733e+07,0.119868,0.500408,),(2.5311e+07,0.129926,0.519994,),(2.49816e+07,0.13835,0.54486,),(2.47494e+07,0.144481,0.563174,),(2.45244e+07,0.15128,0.580164,),(2.43013e+07,0.159282,0.596572,),(2.41042e+07,0.162283,0.612681,),(2.43621e+07,0.14888,0.588531,),(2.56596e+07,0.100437,0.396463,),(2.54983e+07,0.105713,0.397374,),(2.54693e+07,0.109349,0.410223,),(2.5257e+07,0.118566,0.429088,),(2.51356e+07,0.126278,0.447582,),(2.50815e+07,0.125193,0.459449,),(2.49517e+07,0.130933,0.477053,),(2.4748e+07,0.141646,0.494295,),(2.45933e+07,0.144869,0.505182,),(2.44262e+07,0.146857,0.520974,),(2.42793e+07,0.15532,0.536539,),(2.41369e+07,0.16114,0.546725,),(2.3931e+07,0.165635,0.556854,),(2.39012e+07,0.165743,0.571374,),(2.36839e+07,0.179127,0.585637,),(2.35424e+07,0.182303,0.59499,),(2.34118e+07,0.181831,0.608591,),(2.32816e+07,0.185741,0.62153,),(2.3098e+07,0.195713,0.634331,),(2.297e+07,0.192984,0.646627,),(2.27227e+07,0.207778,0.658086,),(2.31233e+07,0.186023,0.613195,),(2.44208e+07,0.115602,0.396665,),(2.42431e+07,0.123231,0.393239,),(2.42115e+07,0.128818,0.408342,),(2.4298e+07,0.128329,0.423647,),(2.4243e+07,0.134524,0.433698,),(2.40686e+07,0.146245,0.448383,),(2.40298e+07,0.145011,0.463111,),(2.39279e+07,0.153734,0.477314,),(2.37807e+07,0.159339,0.490964,),(2.37428e+07,0.16105,0.50438,),(2.33494e+07,0.174467,0.521955,),]

xstore_points = [(514062,0.0105858,0.120676,),(3.63517e+07,0.00509541,0.321044,),(3.30936e+07,0.0105033,0.621419,),(2.81584e+07,0.00306203,0.805581,),(2.90666e+07,0.0088136,0.817104,),(3.48908e+07,0.00486459,0.480357,),(3.17858e+07,0.0042596,0.647943,),(2.99972e+07,0.00340555,0.767307,),(3.21188e+07,0.0135812,0.598493,),(3.25451e+07,0.00444362,0.594865,),(3.1239e+07,0.00412085,0.668935,),(2.98452e+07,0.00363983,0.749606,),(2.98499e+07,0.0177899,0.694567,),(3.31434e+07,0.00484097,0.525395,),(3.16681e+07,0.00457616,0.613601,),(3.04072e+07,0.00414431,0.687569,),(2.93771e+07,0.00374342,0.745185,),(2.83357e+07,0.00339066,0.793584,),(2.80891e+07,0.0245744,0.747658,),(3.33331e+07,0.00583511,0.490321,),(3.25515e+07,0.00465348,0.535555,),(3.15436e+07,0.0044183,0.595131,),(3.0622e+07,0.00415365,0.647688,),(2.96859e+07,0.00385157,0.7017,),(2.85244e+07,0.00365795,0.759271,),(2.78636e+07,0.0034403,0.781333,),(2.74893e+07,0.00324137,0.801017,),(2.68916e+07,0.0201327,0.800125,),(3.2243e+07,0.0277037,0.4929,),(3.1841e+07,0.00488012,0.541941,),(3.11408e+07,0.00461093,0.588684,),(3.05985e+07,0.00456281,0.617037,),(2.99319e+07,0.00437093,0.65539,),(2.93213e+07,0.0042118,0.690768,),(2.89085e+07,0.0040576,0.712238,),(2.83415e+07,0.00384903,0.741271,),(2.76464e+07,0.00363176,0.76757,),(2.72731e+07,0.00349114,0.783456,),(2.68056e+07,0.00328779,0.804775,),(2.58509e+07,0.00311131,0.823856,),(3.06243e+07,0.0415981,0.536446,),(3.22526e+07,0.00490833,0.499632,),(3.16441e+07,0.00465966,0.534731,),(3.10375e+07,0.00455622,0.567652,),(3.06165e+07,0.00450825,0.587943,),(2.96987e+07,0.00438813,0.628774,),(2.89147e+07,0.00419315,0.664581,),(2.83998e+07,0.00401986,0.688621,),(2.80696e+07,0.00393627,0.703714,),(2.77123e+07,0.00382446,0.724838,),(2.72733e+07,0.00367401,0.744804,),(2.70811e+07,0.00353444,0.762847,),(2.67394e+07,0.00342236,0.779868,),(2.63866e+07,0.00322441,0.794969,),(2.61e+07,0.00305331,0.809274,),(2.59315e+07,0.00296595,0.817879,),(2.53381e+07,0.0305297,0.800096,),(3.07071e+07,0.0588962,0.498545,),(3.11393e+07,0.005852,0.524796,),(3.07586e+07,0.00468989,0.5499,),(3.0356e+07,0.00461797,0.574184,),(2.9932e+07,0.00452764,0.597384,),(2.96548e+07,0.00444548,0.611714,),(2.91431e+07,0.00436791,0.632489,),(2.87507e+07,0.00433631,0.652174,),(2.84862e+07,0.00417473,0.664781,),(2.81157e+07,0.00410271,0.682603,),(2.78418e+07,0.00399346,0.699515,),(2.75476e+07,0.00395941,0.715633,),(2.72413e+07,0.00379922,0.730923,),(2.69942e+07,0.00378397,0.74048,),(2.68946e+07,0.00365185,0.74954,),(2.66349e+07,0.00355411,0.762535,),(2.62889e+07,0.00349167,0.774926,),(2.60264e+07,0.00336128,0.786509,),(2.56545e+07,0.00323715,0.797487,),(2.54126e+07,0.00314772,0.8079,),(2.52152e+07,0.00302888,0.817646,),(2.40756e+07,0.00299973,0.823839,),(2.93839e+07,0.0725515,0.518564,),(3.13447e+07,0.00703901,0.484242,),(3.09881e+07,0.00511015,0.504464,),(3.06476e+07,0.00478908,0.524091,),(3.0305e+07,0.00467875,0.536464,),(2.99723e+07,0.00461498,0.554433,),(2.95846e+07,0.0045848,0.571802,),(2.95058e+07,0.00463231,0.583492,),(2.91713e+07,0.00444929,0.599726,),(2.87778e+07,0.00435342,0.615243,),(2.84242e+07,0.00429463,0.630056,),(2.81652e+07,0.00425918,0.644191,),(2.79607e+07,0.00418688,0.658078,),(2.77239e+07,0.00411606,0.671382,),(2.7512e+07,0.00398317,0.679489,),(2.73092e+07,0.00397254,0.691821,),(2.71317e+07,0.0038871,0.703363,),(2.68708e+07,0.00378926,0.714448,),(2.66098e+07,0.0037292,0.72527,),(2.63155e+07,0.00366605,0.736384,),(2.57841e+07,0.00359953,0.760738,),(2.57295e+07,0.00352858,0.766987,),(2.54772e+07,0.0034324,0.77598,),(2.53292e+07,0.00338343,0.784569,),(2.46652e+07,0.00331233,0.792697,),(2.57415e+07,0.0922443,0.668329,),(3.01419e+07,0.077565,0.438532,),(3.11559e+07,0.01046,0.455331,),(3.09586e+07,0.00549941,0.471965,),(3.06403e+07,0.00510859,0.482844,),(3.05192e+07,0.00488311,0.498731,),(3.02171e+07,0.00482338,0.513968,),(2.98775e+07,0.00480192,0.523996,),(2.96547e+07,0.00471115,0.538691,),(2.93148e+07,0.00465251,0.55253,),(2.89855e+07,0.00462332,0.566041,),(2.87509e+07,0.00453308,0.579516,),(2.85784e+07,0.00450105,0.592148,),(2.83894e+07,0.00448096,0.604409,),(2.82378e+07,0.00449986,0.612498,),(2.80978e+07,0.00439288,0.624394,),]

xstore_points = [(1.15692e+06,0.00809596,0.162239,),(3.27214e+07,0.0046624,0.328652,),(2.99423e+07,0.010799,0.73313,),(2.82214e+07,0.00279707,0.844343,),(2.7107e+07,0.00183794,0.914431,),(2.90231e+07,0.0186802,0.739706,),(3.17716e+07,0.00440761,0.607388,),(3.04024e+07,0.00396587,0.693808,),(2.85204e+07,0.00326266,0.787831,),(2.75462e+07,0.0208921,0.807417,),(3.35931e+07,0.00580581,0.499727,),(3.14724e+07,0.00404178,0.614492,),(2.94879e+07,0.00378043,0.704693,),(2.83463e+07,0.0033386,0.771467,),(3.17797e+07,0.0278632,0.506456,),(3.29178e+07,0.00467911,0.492925,),(3.14663e+07,0.00443464,0.581487,),(3.02346e+07,0.00411608,0.65759,),(2.90976e+07,0.00379622,0.718567,),(2.79101e+07,0.00344341,0.770044,),(2.87111e+07,0.0390463,0.638513,),(3.25297e+07,0.00477786,0.487878,),(3.15154e+07,0.00437284,0.556385,),(3.08323e+07,0.00431228,0.597539,),(2.9847e+07,0.00402485,0.651358,),(2.90104e+07,0.00387018,0.699451,),(2.85869e+07,0.0216012,0.671895,),(3.33895e+07,0.00710837,0.388894,),(3.28737e+07,0.0047217,0.430868,),(3.2113e+07,0.00460044,0.4892,),(3.12727e+07,0.00447528,0.543338,),(3.08201e+07,0.00442309,0.575804,),(3.00998e+07,0.00432497,0.62065,),(2.93585e+07,0.00420717,0.661709,),(2.85732e+07,0.00415114,0.686607,),(3.03006e+07,0.0490534,0.480123,),(3.2538e+07,0.00516847,0.411768,),(3.20005e+07,0.00474091,0.461046,),(3.16268e+07,0.00472239,0.491886,),(3.09102e+07,0.00461617,0.535213,),(3.04017e+07,0.00450493,0.575381,),(2.97627e+07,0.00442369,0.612222,),(2.91147e+07,0.00424837,0.646225,),(2.8767e+07,0.00417389,0.667115,),(2.81483e+07,0.00413821,0.686701,),(2.86316e+07,0.057698,0.554486,),(3.21619e+07,0.00746894,0.399216,),(3.15993e+07,0.00470497,0.438578,),(3.15198e+07,0.00461513,0.464042,),(3.09704e+07,0.00452263,0.499003,),(3.0509e+07,0.004476,0.532694,),(3.02291e+07,0.00445802,0.553819,),(2.97823e+07,0.00435211,0.583908,),(2.92999e+07,0.00429324,0.61235,),(2.90177e+07,0.00429737,0.630383,),(2.84473e+07,0.00414889,0.655479,),(2.79317e+07,0.00404191,0.678923,),(2.79537e+07,0.0387799,0.598477,),(3.15223e+07,0.0105267,0.391013,),(3.1223e+07,0.0048318,0.422389,),(3.08725e+07,0.00465226,0.453176,),(3.05861e+07,0.00456889,0.481807,),(3.02354e+07,0.00451757,0.509425,),(2.99419e+07,0.00450942,0.535498,),(2.96561e+07,0.00450523,0.561079,),(2.9336e+07,0.00441811,0.584864,),(2.90353e+07,0.00440292,0.607944,),(2.86681e+07,0.00428151,0.629513,),(2.82931e+07,0.00425287,0.650306,),(2.77806e+07,0.00417196,0.66976,),(2.83541e+07,0.0785868,0.476531,),(3.11891e+07,0.00941837,0.372181,),(3.10452e+07,0.00484688,0.398709,),(3.07304e+07,0.0047698,0.415804,),(3.0627e+07,0.0046867,0.440555,),(3.03596e+07,0.00468961,0.464971,),(3.01316e+07,0.00461604,0.480289,),(2.98323e+07,0.00463893,0.502812,),(2.95546e+07,0.00456772,0.524603,),(2.92094e+07,0.00458952,0.545614,),(2.90715e+07,0.00457522,0.566056,),(2.87951e+07,0.00446032,0.585143,),(2.84452e+07,0.00442248,0.604032,),(2.81851e+07,0.00437011,0.621408,),(2.79341e+07,0.00431711,0.638328,),(2.779e+07,0.00426607,0.64945,),(2.75388e+07,0.00420392,0.665177,),(2.72674e+07,0.00415979,0.680431,),(2.69921e+07,0.00414226,0.689951,),(2.69105e+07,0.102168,0.516611,),(3.00631e+07,0.0156882,0.410989,),(3.00381e+07,0.00520261,0.431374,),(2.98857e+07,0.00476134,0.451,),(2.96903e+07,0.00468253,0.470266,),(2.95238e+07,0.00463974,0.482865,),(2.93528e+07,0.00460154,0.501041,),(2.9149e+07,0.00456013,0.519029,),(2.89183e+07,0.00450577,0.535949,),(2.86596e+07,0.00447627,0.552356,),(2.84936e+07,0.00443407,0.568566,),(2.82957e+07,0.00441963,0.584082,),(2.80618e+07,0.00439366,0.59897,),(2.78808e+07,0.00437737,0.614006,),(2.78222e+07,0.00431669,0.623357,),(2.76511e+07,0.00427633,0.636949,),(2.73828e+07,0.00424304,0.650526,),(2.72452e+07,0.00415648,0.65889,),(2.7033e+07,0.00415508,0.671359,),(2.6833e+07,0.00406526,0.683301,),(2.66591e+07,0.00401401,0.695101,),(2.64843e+07,0.00400989,0.706003,),(2.63094e+07,0.00395733,0.717065,),(2.58983e+07,0.00392092,0.727745,),(2.77036e+07,0.0762367,0.451855,),(2.94111e+07,0.00876461,0.424014,),(2.9427e+07,0.00544263,0.440001,),(2.92115e+07,0.00471723,0.455422,),(2.9112e+07,0.00469245,0.470821,),(2.89483e+07,0.00461046,0.485765,),(2.88044e+07,0.00456207,0.500276,),]

rpc_points = [(590238,0,1.00002,),(2.73898e+07,0,1,),(2.71808e+07,0,1,),(2.68405e+07,0,1,),(2.68441e+07,0,1,),(2.66235e+07,0,1,),(2.64776e+07,0,1,),(2.6348e+07,0,1,),(2.64073e+07,0,1,),(2.60649e+07,0,1,),(2.60718e+07,0,0.999999,),(2.58473e+07,0,1,),(2.55734e+07,0,1,),(2.5541e+07,0,1,),(2.54238e+07,0,1,),(2.52956e+07,0,1,),(2.51108e+07,0,1,),(2.49636e+07,0,1,),(2.50829e+07,0,1,),(2.49716e+07,0,1,),(2.47869e+07,0,1,),(2.46479e+07,0,1,),(2.45213e+07,0,1,),(2.4426e+07,0,1,),(2.43211e+07,0,1,),(2.41386e+07,0,1,),(2.41113e+07,0,1,),(2.41182e+07,0,1,),(2.41994e+07,0,1,),(2.41402e+07,0,1,),(2.40013e+07,0,1,),(2.38296e+07,0,1,),(2.3786e+07,0,1,),(2.38211e+07,0,1,),(2.38048e+07,0,1,),(2.36584e+07,0,1,),(2.33558e+07,0,1,),(2.35314e+07,0,1,),(2.34966e+07,0,1,),(2.35271e+07,0,1,),(2.34861e+07,0,1,),(2.33148e+07,0,1,),(2.32169e+07,0,1,),(2.30603e+07,0,1,),(2.29532e+07,0,1,),(2.28858e+07,0,1,),(2.29109e+07,0,1,),(2.28228e+07,0,1,),(2.28164e+07,0,1,),(2.28325e+07,0,1,),(2.28058e+07,0,1,),(2.26334e+07,0,1,),(2.27047e+07,0,1,),(2.2678e+07,0,1,),(2.25395e+07,0,1,),(2.26262e+07,0,1,),(2.26443e+07,0,1,),(2.2624e+07,0,1,),(2.26732e+07,0,1,),(2.2639e+07,0,1,),(2.25349e+07,0,1,),(2.23206e+07,0,1,),(2.24318e+07,0,1,),(2.22365e+07,0,1,),(2.2479e+07,0,1,),(2.24008e+07,0,1,),(2.23707e+07,0,1,),(2.24249e+07,0,1,),(2.24235e+07,0,1,),(2.23804e+07,0,1,),(2.23291e+07,0,1,),(2.23353e+07,0,1,),(2.21549e+07,0,1,),(2.21187e+07,0,1,),(2.21706e+07,0,1,),(2.19623e+07,0,1,),(2.21666e+07,0,1,),(2.21125e+07,0,1,),(2.21037e+07,0,1,),(2.20707e+07,0,1,),(2.20362e+07,0,1,),(2.21122e+07,0,1,),(2.21248e+07,0,1,),(2.20567e+07,0,1,),(2.2074e+07,0,0.999999,),(2.20456e+07,0,1,),(2.19434e+07,0,1,),(2.16898e+07,0,1,),(2.18587e+07,0,1,),(2.18957e+07,0,1,),(2.16723e+07,0,1,),(2.18626e+07,0,1,),(2.18872e+07,0,1,),(2.18214e+07,0,0.999999,),(2.18367e+07,0,1,),(2.18431e+07,0,1,),(2.17502e+07,0,1,),(2.18229e+07,0,1,),(2.18719e+07,0,1,),(2.18696e+07,0,1,),(2.18651e+07,0,1,),(2.18647e+07,0,1,),(2.18023e+07,0,1,),(2.16976e+07,0,1,),(2.15072e+07,0,1,),(2.16325e+07,0,1,),(2.16004e+07,0,1,),(2.1688e+07,0,1,),(2.15006e+07,0,1,),(2.1598e+07,0,1,),(2.16209e+07,0,1,),(2.16497e+07,0,1,),(2.15649e+07,0,1,),(2.15776e+07,0,1,),(2.15993e+07,0,1,),(2.14985e+07,0,1,),(2.14941e+07,0,1,),(2.15959e+07,0,1,),(2.15464e+07,0,1,),(2.15924e+07,0,1,),]


"""
2019.10.31 replot using HTM to protect the Get()
"""
xstore_points = [(2.34182e+06,0.00950512,0.475349,),(2.76334e+07,0.00349884,0.786508,),(2.79237e+07,0.0112088,0.750211,),(2.67774e+07,0.00321881,0.802728,),(2.4846e+07,0.00239568,0.876517,),(3.13843e+07,0.0114546,0.530559,),(3.05265e+07,0.00405308,0.587178,),(2.82075e+07,0.00348472,0.680293,),(2.59423e+07,0.00277976,0.748251,),(3.14721e+07,0.0145538,0.504324,),(2.9001e+07,0.00427642,0.611765,),(2.68261e+07,0.00398053,0.688545,),(2.51285e+07,0.00362497,0.751467,),(2.56825e+07,0.0308898,0.667718,),(3.20413e+07,0.00524695,0.453362,),(2.9907e+07,0.00451558,0.540169,),(2.85563e+07,0.00442278,0.589042,),(2.67266e+07,0.00414555,0.650703,),(2.50895e+07,0.00383869,0.703623,),(3.01352e+07,0.0189789,0.463256,),(3.15669e+07,0.00456247,0.448424,),(3.04248e+07,0.00450358,0.493058,),(2.85384e+07,0.00440254,0.552072,),(2.72259e+07,0.00420803,0.604586,),(2.62398e+07,0.00408652,0.635225,),(2.49477e+07,0.00397614,0.675896,),(2.58288e+07,0.0458631,0.57798,),(3.1235e+07,0.00592726,0.420134,),(2.97831e+07,0.00458263,0.475927,),(2.83735e+07,0.00456692,0.52485,),(2.7179e+07,0.0044849,0.570033,),(2.61207e+07,0.00433505,0.609489,),(2.51877e+07,0.0042468,0.645589,),(2.39825e+07,0.00418683,0.677899,),(2.87505e+07,0.0501054,0.435425,),(3.10702e+07,0.00513034,0.39814,),(2.99311e+07,0.00474298,0.445211,),(2.86941e+07,0.00468059,0.487094,),(2.73816e+07,0.00461442,0.526191,),(2.67474e+07,0.00457766,0.550144,),(2.5717e+07,0.00444306,0.58267,),(2.46957e+07,0.00442184,0.613297,),(2.40343e+07,0.00426336,0.640737,),(2.32344e+07,0.00417308,0.666182,),(2.61561e+07,0.0363263,0.499233,),(3.02978e+07,0.00520747,0.395835,),(2.9296e+07,0.00463992,0.432085,),(2.85206e+07,0.00457819,0.466087,),(2.75879e+07,0.00453248,0.497396,),(2.69098e+07,0.00448921,0.527572,),(2.62867e+07,0.00440532,0.555009,),(2.56924e+07,0.00436837,0.581554,),(2.50507e+07,0.00433335,0.59762,),(2.44694e+07,0.00422202,0.620511,),(2.40485e+07,0.00424965,0.64233,),(2.35772e+07,0.00410406,0.662784,),(2.33272e+07,0.0351862,0.642062,),(3.0323e+07,0.0254866,0.371856,),(3.02885e+07,0.00475069,0.403414,),(2.94781e+07,0.00462908,0.433016,),(2.89076e+07,0.00461041,0.4614,),(2.82576e+07,0.00452084,0.487644,),(2.76013e+07,0.00456332,0.513221,),(2.714e+07,0.00452187,0.537126,),(2.6767e+07,0.00445167,0.559989,),(2.62884e+07,0.00443118,0.581562,),(2.56354e+07,0.00438439,0.602541,),(2.5126e+07,0.00437119,0.621881,),(2.46701e+07,0.00422276,0.640443,),(2.41625e+07,0.00424552,0.657894,),(2.32791e+07,0.00415828,0.674443,),(2.86313e+07,0.0473917,0.402609,),(2.97829e+07,0.0053077,0.394604,),(2.92046e+07,0.00474645,0.419094,),(2.89222e+07,0.00465358,0.443026,),(2.85877e+07,0.0046185,0.458205,),(2.81214e+07,0.00461563,0.480225,),(2.76192e+07,0.00459703,0.501384,),(2.7359e+07,0.00465316,0.515327,),(2.70947e+07,0.00460922,0.528736,),(2.66161e+07,0.00453324,0.547821,),(2.61963e+07,0.00449609,0.566028,),(2.58369e+07,0.00449485,0.583729,),(2.54428e+07,0.00445543,0.600727,),(2.51455e+07,0.0043795,0.611648,),(2.46813e+07,0.00431542,0.626997,),(2.42817e+07,0.00428858,0.642028,),(2.39558e+07,0.00420567,0.655948,),(2.36652e+07,0.00416592,0.669413,),(2.33595e+07,0.00416682,0.678056,),(2.28332e+07,0.00824672,0.686724,),(2.78457e+07,0.103111,0.384011,),(2.89632e+07,0.0114762,0.397567,),(2.88112e+07,0.00537795,0.417674,),(2.84603e+07,0.0047726,0.436957,),(2.80704e+07,0.00467309,0.449451,),(2.77733e+07,0.00465347,0.467676,),(2.74597e+07,0.00461184,0.485153,),(2.71197e+07,0.0045606,0.50222,),(2.67662e+07,0.00451947,0.518483,),(2.66875e+07,0.00449249,0.528972,),(2.64998e+07,0.0045104,0.544493,),(2.61849e+07,0.00447628,0.559693,),(2.57768e+07,0.0043796,0.573931,),(2.5404e+07,0.00437531,0.587799,),(2.53349e+07,0.00434187,0.597072,),(2.50832e+07,0.00432324,0.610152,),(2.49171e+07,0.00433348,0.62316,),(2.44712e+07,0.00424702,0.635254,),(2.43537e+07,0.00424565,0.647323,),(2.41273e+07,0.00419333,0.654735,),(2.39268e+07,0.00413689,0.66603,),(2.36344e+07,0.00412458,0.676722,),(2.32263e+07,0.00408734,0.687377,),(2.30503e+07,0.00407616,0.697362,),(2.31343e+07,0.0642697,0.599172,),(2.82941e+07,0.0357332,0.391584,),(2.84904e+07,0.00765971,0.407953,),(2.84664e+07,0.0050504,0.423965,),(2.82353e+07,0.004807,0.434398,),(2.75652e+07,0.00467749,0.449309,),(2.76878e+07,0.00458896,0.464126,),(2.75651e+07,0.004545,0.473661,),(2.72734e+07,0.00454755,0.487717,),(2.70639e+07,0.0045665,0.501553,),(2.67911e+07,0.00457942,0.510502,),(2.63988e+07,0.00452459,0.523739,),(2.61287e+07,0.00445594,0.536612,),(2.59273e+07,0.00445256,0.548763,),(2.56767e+07,0.00444137,0.561,),(2.5541e+07,0.00440083,0.57262,),(2.54841e+07,0.00435479,0.584046,),(2.53418e+07,0.00432956,0.591326,),(2.514e+07,0.00432539,0.60262,),(2.48786e+07,0.00428949,0.61316,),]

rpc_points = [(332847,0,1.00003,),(2.47414e+07,0,1,),(2.42827e+07,0,1,),(2.38927e+07,0,1,),(2.35845e+07,0,1,),(2.33644e+07,0,1,),(2.29636e+07,0,1,),(2.27361e+07,0,1,),(2.25722e+07,0,1,),(2.19536e+07,0,1,),(2.1656e+07,0,1,),(2.17402e+07,0,1,),(2.15794e+07,0,1,),(2.13216e+07,0,1,),(2.06874e+07,0,1,),(2.02576e+07,0,1,),(2.04552e+07,0,1,),(2.01518e+07,0,1,),(2.00155e+07,0,1,),(1.96474e+07,0,1,),(1.95181e+07,0,1,),(1.9591e+07,0,1,),(1.94416e+07,0,1,),(1.91227e+07,0,1,),(1.90649e+07,0,0.999999,),(1.89259e+07,0,1,),(1.86432e+07,0,1,),(1.87868e+07,0,1,),(1.86522e+07,0,1,),(1.85479e+07,0,1,),(1.84332e+07,0,1,),(1.83288e+07,0,1,),(1.82341e+07,0,1,),(1.82593e+07,0,1,),(1.82808e+07,0,1,),(1.82397e+07,0,1,),(1.81366e+07,0,1,),(1.80542e+07,0,1,),(1.77868e+07,0,1,),(1.77468e+07,0,1,),(1.76926e+07,0,1,),(1.76014e+07,0,1,),(1.76701e+07,0,1,),(1.75987e+07,0,1,),(1.75196e+07,0,1,),(1.6906e+07,0,0.999999,),(1.73219e+07,0,1,),(1.71419e+07,0,1,),(1.72641e+07,0,1,),(1.72974e+07,0,1,),(1.72595e+07,0,1,),(1.71943e+07,0,1,),(1.71918e+07,0,1,),(1.71689e+07,0,1,),(1.66775e+07,0,1,),(1.70209e+07,0,1,),(1.69334e+07,0,1,),(1.69874e+07,0,1,),(1.68868e+07,0,1,),(1.68933e+07,0,1,),(1.69061e+07,0,1,),(1.69074e+07,0,1,),(1.68666e+07,0,1,),(1.68423e+07,0,1,),(1.60296e+07,0,1,),(1.66928e+07,0,1,),(1.60893e+07,0,1,),(1.66813e+07,0,1,),(1.66807e+07,0,1,),(1.65688e+07,0,1,),(1.66389e+07,0,1,),(1.6628e+07,0,1,),(1.65852e+07,0,1,),(1.65319e+07,0,1,),(1.61118e+07,0,1,),(1.6483e+07,0,1,),(1.64179e+07,0,1,),(1.61542e+07,0,1,),(1.65907e+07,0,1,),(1.65675e+07,0,1,),(1.6217e+07,0,1,),(1.65374e+07,0,1,),(1.65122e+07,0,1,),(1.64802e+07,0,1,),(1.64908e+07,0,1,),(1.66485e+07,0,1,),(1.59723e+07,0,1,),(1.65286e+07,0,1,),(1.64753e+07,0,1,),(1.60857e+07,0,1,),(1.64963e+07,0,1,),(1.64721e+07,0,1,),(1.66348e+07,0,1,),(1.66551e+07,0,1,),(1.66107e+07,0,1,),(1.6689e+07,0,1,),(1.66937e+07,0,1,),(1.66961e+07,0,1,),(1.6639e+07,0,1,),(1.64701e+07,0,1,),(1.64727e+07,0,1,),(1.661e+07,0,1,),(1.64756e+07,0,1,),(1.66965e+07,0,1,),(1.66737e+07,0,1,),(1.66591e+07,0,1,),(1.6622e+07,0,1,),(1.66657e+07,0,1,),(1.65954e+07,0,1,),(1.66721e+07,0,1,),(1.66217e+07,0,1,),(1.65829e+07,0,0.999999,),(1.65839e+07,0,1,),(1.65493e+07,0,1,),(1.65259e+07,0,1,),(1.65634e+07,0,1,),(1.65061e+07,0,1,),(1.64291e+07,0,1,),(1.63823e+07,0,1,),(1.64038e+07,0,1,),(1.64284e+07,0,1,),(1.64314e+07,0,1,),(1.63011e+07,0,0.999999,),(1.63574e+07,0,1,),(1.64195e+07,0,1,),(1.63357e+07,0,1,),(1.63582e+07,0,1,),(1.634e+07,0,1,),(1.62914e+07,0,1,),(1.62549e+07,0,1,),(1.63007e+07,0,1,),(1.63087e+07,0,1,),(1.62988e+07,0,1,),(1.62219e+07,0,1,),(1.6193e+07,0,1,),]


xstore_thpt = extract_one_dim(xstore_points,0)[0:-5]
xstore_invalid = extract_one_dim(xstore_points,1)
xstore_fallback = extract_one_dim(xstore_points,2)

rpc_thpt = extract_one_dim(rpc_points,0)[0:-5]


data = [xstore_thpt,rpc_thpt]
data1 = [xstore_invalid[10:-5],xstore_fallback[10:-5]]

#ylim = 1
#data = data1
ylabel = "Thpt"
legends = ["Xstore","RPC"]
#ylim = 4.83285e+07

def main():
    output_aligned_lines("ycsbd_i",data,legends)
    output_aligned_lines("ycsbd_i_ratio",data1,["Slowdown","Fallback"])

if __name__ == "__main__":
    main()