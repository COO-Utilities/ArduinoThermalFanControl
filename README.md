# Arduino Thermal Fan Control

Python client for the Arduino sketch in `ArduinoThermalFanControl.ino`.

## Install

```bash
python -m pip install -r requirements.txt
python -m pip install -e .
```

## Usage

```python
from arduino_thermal_fan_control import connect

controller = connect(vid=0x2341, pid=0x0069)
try:
    controller.set_fan_speed(75)
    controller.set_heater(False)
finally:
    controller.close()
```

If you already know the serial port:

```python
from arduino_thermal_fan_control import ThermalFanController

with ThermalFanController(port="/dev/ttyACM0") as controller:
    controller.set_fan_speed(50)
    controller.set_heater(True)
```
