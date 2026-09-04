from arduino_thermal_fan_control import connect


def main() -> None:
    controller = connect(vid=0x2341, pid=0x0069)
    try:
        print(controller.set_fan_speed(40))
        print(controller.set_motor_heater(True))
        print(controller.set_motor_heater(False))
        print(controller.set_reject_heater(True))
        print(controller.set_reject_heater(False))
    finally:
        controller.close()


if __name__ == "__main__":
    main()
