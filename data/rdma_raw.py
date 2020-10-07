#!/usr/bin/env python

import numpy as np
import matplotlib.pyplot as plt

payloads = [8,16,64,128,256,384,1024]
perf = [107350709.86,114063706.98,74915589.31,59494883.92,37042282.52,26654064.47,11096394.09]

bandwidth = [ (100 * 1000 * 1000 * 1000 / 8.0) for i in range(len(perf))]
bandwidths = [ payloads[i] * perf[i] for i in range(len(perf))]

x = np.linspace(0, 10, 500)
y = np.sin(x)

fig, ax = plt.subplots()

# Using set_dashes() to modify dashing of an existing line
#line1, = ax.plot(x, y, label='Using set_dashes()')
#line1.set_dashes([2, 2, 10, 2])  # 2pt line, 2pt break, 10pt line, 2pt break

# Using plot(..., dashes=...) to set the dashing when creating a line
#line2, = ax.plot(x, y - 0.2, dashes=[6, 2], label='Using the dashes parameter')
line2, = ax.plot(payloads,perf,label='RDMA reqs throughput')
ax.set_ylabel("Throughput reqs/sec")
ax.set_xlabel("Payload (bytes)")

ax2 = ax.twinx()
line3, = ax2.plot(payloads,bandwidth,label='ideal bandwidth')
line4, = ax2.plot(payloads,bandwidths,label='bandwidth of requests')
line3.set_color("red")
ax2.set_ylabel("Bandwidth bps")

ax.legend(loc = 'center right')
ax2.legend()
plt.show()
