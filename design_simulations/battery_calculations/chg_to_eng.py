import csv
import numpy as np
import matplotlib.pyplot as plt


with open('charge-0.2a.csv', 'r') as f:
    chg_data = np.array([x for x in csv.reader(f)]).astype(float).transpose()

with open('energy-0.2a.csv', 'r') as f:
    eng_data = np.array([x for x in csv.reader(f)]).astype(float).transpose()

chg_data_true = np.max(chg_data[1]) - chg_data[1]
eng_data_true = np.max(eng_data[1]) - eng_data[1]

poly, stats = np.polynomial.Polynomial.fit(chg_data_true, eng_data_true, 2, full=True)
poly = poly.convert()
eng_data_interp = poly(chg_data_true)
print('Ah-to-Wh:', poly)
print(stats)

with open('charge-1.0a.csv', 'r') as f:
    chg_data_h = np.array([x for x in csv.reader(f)]).astype(float).transpose()

with open('energy-1.0a.csv', 'r') as f:
    eng_data_h = np.array([x for x in csv.reader(f)]).astype(float).transpose()

chg_data_true_h = np.max(chg_data_h[1]) - chg_data_h[1]
eng_data_true_h = np.max(eng_data_h[1]) - eng_data_h[1]

plt.scatter(chg_data_true, eng_data_true)
plt.scatter(chg_data_true_h, eng_data_true_h)
plt.plot(chg_data_true, eng_data_interp, color='green')
plt.show()
