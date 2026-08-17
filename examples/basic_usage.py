from arduino_thermal_fan_control import connect


def main() -> None:
    controller = connect(vid=0x2341, pid=0x0069)
    try:
        print(controller.set_fan_speed(40))
        print(controller.set_heater(True))
        print(controller.set_heater(False))
    finally:
        controller.close()


if __name__ == "__main__":
    main()
