import serial, time
s = serial.Serial('/dev/ttyACM0', 9600, timeout=2)
time.sleep(2)                      # board settles
s.write(b'STATUS\n')
print(s.readline())
