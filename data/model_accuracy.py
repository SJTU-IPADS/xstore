#!/usr/bin/env python

import matplotlib.pyplot as plt
import numpy as np

from model_accuracy_data import all_data

x = np.arange(3)
y = 2.5 * np.sin(x / 20 * np.pi)

yerr = np.array([[0.05,0.05,0.05],[0.2,0.2,0.2]])

#plt.errorbar(x, y + 3, yerr=yerr, label='both limits (default)')
ycsbh = all_data["ycsbh"]
plt.errorbar(ycsbh.x,ycsbh.y,yerr = ycsbh.get_err(),label = ycsbh.label)
plt.ylim(0,1000)

plt.show()
