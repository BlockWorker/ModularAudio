import csv
import numpy as np
import matplotlib.pyplot as plt
from scipy.interpolate import make_splrep


with open('energy-0.2a.csv', 'r') as f:
    eng_data = np.array([x for x in csv.reader(f)]).astype(float).transpose()

volt_data = eng_data[0]
eng_data_true = (np.max(eng_data[1]) - eng_data[1])

spl = make_splrep(np.array(volt_data), eng_data_true, s=.01)
eng_data_interp = spl(volt_data)
print(spl.t, len(spl.t))
print(spl.c, len(spl.c))

#with open('test.csv', 'r') as f:
#    test_data = np.array([x for x in csv.reader(f)]).astype(float).transpose()

plt.scatter(volt_data, eng_data_true)
#plt.scatter(test_data[0], test_data[1])
plt.plot(volt_data, eng_data_interp, color='red')
plt.show()
