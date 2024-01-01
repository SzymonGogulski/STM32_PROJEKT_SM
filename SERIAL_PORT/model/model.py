# Import statements
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import control as ct
import scipy.signal as sig
import os
os.system("cls")

# Measurement file paths
current_directory = os.path.dirname(__file__)
parent_directory = os.path.join(current_directory, "..")
parent_directory = os.path.abspath(parent_directory)
PWM12_path = os.path.join(parent_directory, "pomiary\samples_21-42-36_PWM12.5%.csv")
PWM10_path = os.path.join(parent_directory, "pomiary\samples_23-26-35_PWM10%.csv")

# Measurement dataframes
columns = ['Czas','Temperatura']
PWM10 = pd.read_csv(PWM10_path, header=None, names=columns, sep=';')
PWM12 = pd.read_csv(PWM12_path, header=None, names=columns, sep=';')

# Mathematical model
input10 = 0.1
input12 = 0.125

k = 10
k10 = k/input10
k12 = k/input12
T = 300
delay = 10

M10 = sig.TransferFunction([k10], [T, 1])
print(sig.pade(M10, 2))
print(M10)

# s = ct.TransferFunction.s
# delay = ct.pade(delay, 1)
# delay = ct.TransferFunction(delay[0], delay[1])
# M10 = k10/(1+s*T)*delay
# M12 = k12/(1+s*T) * delay
# print(type(PWM10.Czas))
# response10 = ct.input_output_response(M10, PWM10.Czas.array, input10*np.ones_like(PWM10.Czas))


plt.plot(PWM10.Czas, PWM10.Temperatura, label="real PWM 10%")
plt.plot(PWM12.Czas, PWM12.Temperatura, label="real PWM 12.5%")

plt.plot(PWM10.Czas, response10, label="model PWM 10%")

plt.title("Odp. skokowa układu i modelowana")
plt.grid()
plt.legend()
plt.show()

print("Success. ")