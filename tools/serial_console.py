#!/usr/bin/env python3
"""Interactive ESP32-C3 serial console for checks and motor calibration."""

import argparse
import threading

import serial
from serial.tools import list_ports


def choose_port(requested):
    if requested:
        return requested
    matches = []
    for port in list_ports.comports():
        description = f"{port.device} {port.description} {port.manufacturer}".lower()
        if any(
            name in description
            for name in (
                "ttyacm",
                "ttyusb",
                "cp210",
                "ch340",
                "espressif",
                "usb jtag",
                "usb serial",
            )
        ):
            matches.append(port.device)
    if len(matches) != 1:
        raise RuntimeError(
            "Use --port with the correct device. List ports with python -m serial.tools.list_ports -v."
        )
    return matches[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--port", help="USB serial device, such as /dev/ttyACM0 or COM5"
    )
    args = parser.parse_args()
    stop = threading.Event()
    with serial.Serial(
        choose_port(args.port), 115200, timeout=0.2, write_timeout=1
    ) as device:
        print(
            "Connected. Opening the port may reset the board and run its boot movement."
        )
        print(
            "Commands: PING, STATUS, CAL ON, EYE <deg>, ROLL <deg>, HEAD <steps>, CAL OFF."
        )
        print(
            "Close GLaDOS and the Arduino Serial Monitor first. Type quit to close this console."
        )

        def reader():
            while not stop.is_set():
                try:
                    line = device.readline().decode("utf-8", errors="replace").strip()
                    if line:
                        print(f"\n[ESP32] {line}", flush=True)
                except serial.SerialException:
                    stop.set()
                    return

        thread = threading.Thread(target=reader, daemon=True)
        thread.start()
        try:
            while not stop.is_set():
                command = input("> ").strip()
                if command.lower() in ("quit", "exit"):
                    break
                if command:
                    device.write((command + "\n").encode("ascii"))
        except (KeyboardInterrupt, EOFError):
            print("\nConsole closed.")
        finally:
            stop.set()
            thread.join(timeout=1)


if __name__ == "__main__":
    main()
