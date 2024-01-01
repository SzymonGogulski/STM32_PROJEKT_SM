import serial
import os
import csv
from datetime import datetime
os.system('cls')

ser = serial.Serial()
ser.baudrate = 9600
ser.port = 'COM6'
print(ser)

samples = []
it = 0
ser.open()

while(len(samples)<3600):
    line = ser.readline()
    try:
        sample = float(line[0:5].decode("utf-8"))
        it+=1
        print(it, sample)
        samples.append(sample)
    except ValueError:
        print(line)

ser.close()

# Write to the CSV file
current_time = datetime.now().strftime("%H-%M-%S")
file_name = f"samples_{current_time}.csv"

with open(file_name, mode='w', newline='') as csv_file:
    csv_writer = csv.writer(csv_file)
    csv_writer.writerow(samples)

print(f"CSV file '{file_name}' created and values written to it.")

print("Success. ")