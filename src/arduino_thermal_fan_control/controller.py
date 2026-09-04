"""Python client for the Arduino thermal fan controller.

The Arduino sketch speaks a simple line-based protocol over serial:

* ``FAN:<0-100>`` sets the fan duty cycle as a percentage.
* ``MOTORHEATER:ON`` and ``MOTORHEATER:OFF`` control the motor heater relay.
* ``REJECTHEATER:ON`` and ``REJECTHEATER:OFF`` control the reject heater relay.
* ``STATUS`` reports the current temperature and relay/fan states.
* ``SHUTDOWN`` disables the startup relay until the temperature drops below
  then rises above ``STARTUP_TEMP`` again.

This module wraps that protocol in a small serial client and can auto-discover
the Arduino on Linux by USB VID/PID.
"""

from __future__ import annotations

import importlib
from dataclasses import dataclass
from typing import Any, Optional
import time


class ThermalFanControllerError(RuntimeError):
	"""Base exception for controller communication failures."""


class ControllerNotFoundError(ThermalFanControllerError):
	"""Raised when a matching serial port cannot be found."""


class ControllerProtocolError(ThermalFanControllerError):
	"""Raised when the Arduino replies with an unexpected response."""


def _require_pyserial() -> None:
	try:
		importlib.import_module("serial")
		importlib.import_module("serial.tools.list_ports")
	except ImportError as exc:  # pragma: no cover - dependency error path
		raise ImportError(
			"pyserial is required for arduino_thermal_fan_control"
		) from exc


def _get_serial_module() -> Any:
	_require_pyserial()
	return importlib.import_module("serial")


def _get_list_ports_module() -> Any:
	_require_pyserial()
	return importlib.import_module("serial.tools.list_ports")


def _parse_usb_id(value: Optional[int | str]) -> Optional[int]:
	if value is None:
		return None
	if isinstance(value, int):
		return value
	text = str(value).strip().lower()
	if text.startswith("0x"):
		return int(text, 16)
	return int(text, 16) if any(ch in text for ch in "abcdef") else int(text)


def find_serial_port(
	vid: Optional[int | str] = None,
	pid: Optional[int | str] = None,
) -> str:
	"""Find the first matching serial port.

	When ``vid`` and ``pid`` are provided, the function searches for an attached
	device with matching USB identifiers. If either identifier is omitted, the
	first available serial port is returned.
	"""

	_require_pyserial()

	ports = list(_get_list_ports_module().comports())
	if not ports:
		raise ControllerNotFoundError("No serial ports were found")

	target_vid = _parse_usb_id(vid)
	target_pid = _parse_usb_id(pid)

	if target_vid is None or target_pid is None:
		return ports[0].device

	matches = [
		port.device
		for port in ports
		if port.vid == target_vid and port.pid == target_pid
	]

	if not matches:
		raise ControllerNotFoundError(
			f"No serial port matched VID=0x{target_vid:04x} PID=0x{target_pid:04x}"
		)

	return matches[0]


@dataclass
class ThermalFanController:
	"""Serial client for the Arduino thermal fan controller."""

	port: Optional[str] = None
	baudrate: int = 9600
	timeout: float = 1.0
	write_timeout: float = 1.0
	vid: Optional[int | str] = None
	pid: Optional[int | str] = None
	settle_time: float = 2.0

	def __post_init__(self) -> None:
		self._serial = None

	@property
	def is_open(self) -> bool:
		return self._serial is not None and self._serial.is_open

	def open(self) -> "ThermalFanController":
		"""Open the serial connection.

		If ``port`` is not provided, the controller is discovered automatically.
		On Linux, the intended workflow is to pass the Arduino USB VID/PID.
		"""

		if self.is_open:
			return self

		port = self.port
		if not port:
			port = find_serial_port(self.vid, self.pid)

		serial_module = _get_serial_module()
		self._serial = serial_module.Serial(
			port=port,
			baudrate=self.baudrate,
			timeout=self.timeout,
			write_timeout=self.write_timeout,
		)

		time.sleep(self.settle_time)
		self._serial.reset_input_buffer()
		self._serial.reset_output_buffer()
		self.port = port
		return self

	def close(self) -> None:
		if self._serial is not None:
			self._serial.close()
			self._serial = None

	def __enter__(self) -> "ThermalFanController":
		return self.open()

	def __exit__(self, _exc_type, _exc, _tb) -> None:
		del _exc_type, _exc, _tb
		self.close()

	def _ensure_open(self) -> Any:
		if not self.is_open:
			self.open()
		assert self._serial is not None
		return self._serial

	def send_command(self, command: str, read_response: bool = True) -> str:
		"""Send a raw command line to the Arduino.

		The Arduino sketch emits a short status line for each accepted command.
		"""

		connection = self._ensure_open()
		payload = command.strip().encode("ascii") + b"\n"
		connection.write(payload)
		connection.flush()

		if not read_response:
			return ""

		response = connection.readline().decode("utf-8", errors="replace").strip()
		if response:
			return response
		return ""

	def set_fan_speed(self, fan_number: int, percent: int, read_response: bool = True) -> str:
		"""Set the fan duty cycle from 0 to 100 percent."""

		percent = max(0, min(100, int(percent)))
		return self.send_command(f"FAN{fan_number}:{percent}", read_response=read_response)

	def fan_off(self, fan_number: int, read_response: bool = True) -> str:
		"""Convenience helper that turns the fan off."""

		return self.set_fan_speed(fan_number, 0, read_response=read_response)

	def set_motor_heater(self, on: bool, read_response: bool = True) -> str:
		"""Turn the motor heater relay on or off."""

		return self.send_command(
			f"MOTORHEATER:{'ON' if on else 'OFF'}",
			read_response=read_response,
		)

	def set_reject_heater(self, on: bool, read_response: bool = True) -> str:
		"""Turn the reject heater relay on or off."""

		return self.send_command(
			f"REJECTHEATER:{'ON' if on else 'OFF'}",
			read_response=read_response,
		)

	def shutdown(self, read_response: bool = True) -> str:
		"""Disable the startup relay until the temperature drops below then rises above STARTUP_TEMP again."""

		return self.send_command("SHUTDOWN", read_response=read_response)

	def get_status(self) -> dict[str, Any]:
		"""Query the current PT1000 temperature and relay/fan states.

		Sends the ``STATUS`` command and parses the Arduino's
		``STATUS:TEMP=...,MOTORHEATER=...,REJECTHEATER=...,FAN1_PWM=...,FAN2_PWM=...,STARTUP_RELAY=...``
		reply into a dictionary.
		"""

		response = self.send_command("STATUS", read_response=True)
		if not response.startswith("STATUS:"):
			raise ControllerProtocolError(
				f"Unexpected response to STATUS command: {response!r}"
			)
		return response
		# fields = response[len("STATUS:"):].split(",")
		# status: dict[str, Any] = {}
		# for field in fields:
		# 	key, _, value = field.partition("=")
		# 	if key == "TEMP":
		# 		status["temperature_k"] = float(value)
		# 	elif key == "MOTORHEATER":
		# 		status["motor_heater_on"] = value == "ON"
		# 	elif key == "REJECTHEATER":
		# 		status["reject_heater_on"] = value == "ON"
		# 	elif key == "FAN1_PWM":
		# 		status["fan1_percent"] = int(value)
		# 	elif key == "FAN2_PWM":
		# 		status["fan2_percent"] = int(value)
		# 	elif key == "STARTUP_RELAY":
		# 		status["startup_relay_on"] = value == "ON"

		# return status


def connect(
	port: Optional[str] = None,
	*,
	vid: Optional[int | str] = 0x2341,
	pid: Optional[int | str] = 0x0069,
	baudrate: int = 9600,
	timeout: float = 1.0,
	write_timeout: float = 1.0,
	settle_time: float = 2.0,
) -> ThermalFanController:
	"""Create and open a controller connection."""

	controller = ThermalFanController(
		port=port,
		baudrate=baudrate,
		timeout=timeout,
		write_timeout=write_timeout,
		vid=vid,
		pid=pid,
		settle_time=settle_time,
	)
	return controller.open()


__all__ = [
	"ControllerNotFoundError",
	"ControllerProtocolError",
	"ThermalFanController",
	"ThermalFanControllerError",
	"connect",
	"find_serial_port",
]
