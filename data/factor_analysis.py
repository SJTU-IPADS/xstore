#!/usr/bin/env python

import matplotlib.pyplot as plt

data_peak = [
    ("basic", 11.11506972),
    ("+value in index", 15.88131695),
    ("+doorbell batching", 15.87946373),
    ("+ accurate predict", 13.91264604)]

# 1 clients, 6 coroutines per thread, 12 threads
data_low = [("basic", 4.35157071),
            ("+value in index", 6.12056440),
            ("+doorbell batching", 7.22106192),
            ("+accurate predict", 7.07485224)
            ]

peak_array = [v for k, v in data_peak]
peak_label = [k for k, v in data_peak]

low_array = [v for k, v in data_low]
low_label = [k for k, v in data_low]

ax = plt.subplot(1, 2, 1)
ax.set_ylim(0, 20)
bars = plt.bar(range(len(peak_array)), peak_array,
               color=['#B2182B', '#D6604D', '#FDDBC7', '#4393C3'],
               width=0.4, edgecolor='black', linewidth=1)
plt.xticks(range(len(peak_label)), peak_label)
plt.xticks(rotation=15)

ax = plt.subplot(1, 2, 2)
bars = plt.bar(range(len(low_array)), low_array,
               color=['#B2182B', '#D6604D', '#FDDBC7', '#4393C3'],
               width=0.4, edgecolor='black', linewidth=1)
plt.xticks(range(len(low_label)), low_label)
plt.xticks(rotation=15)
ax.set_ylim(0, 20)
# for bar in bars:
# bar.set_linewidth(1)

plt.show()
