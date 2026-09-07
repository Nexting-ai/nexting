#!/usr/bin/env python3
"""Small, dependency-free USB CDC smoke check for a Nexting MultiPad port."""

from __future__ import annotations

import argparse
import os
import select
import termios
import time


def configure(fd: int) -> None:
    attrs = termios.tcgetattr(fd)
    attrs[0] = 0
    attrs[1] = 0
    attrs[2] = termios.CLOCAL | termios.CREAD | termios.CS8
    attrs[3] = 0
    attrs[4] = termios.B115200
    attrs[5] = termios.B115200
    termios.tcsetattr(fd, termios.TCSANOW, attrs)


def read_until(fd: int, timeout: float) -> bytes:
    end = time.monotonic() + timeout
    data = bytearray()
    while time.monotonic() < end:
        ready, _, _ = select.select([fd], [], [], 0.1)
        if ready:
            data.extend(os.read(fd, 4096))
            if b"\n" in data or len(data) >= 3:
                break
    return bytes(data)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("port", help="the explicit USB CDC ACM path")
    parser.add_argument(
        "--present",
        action="store_true",
        help="send a Nexting present frame after the legacy echo check",
    )
    args = parser.parse_args()
    fd = os.open(args.port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    try:
        configure(fd)
        os.write(fd, bytes((0xAA, 0xBB, 0xCC)))
        echo = read_until(fd, 1.5)
        if echo != bytes((0xAA, 0xBB, 0xCC)):
            raise SystemExit(f"legacy CDC echo mismatch: {echo.hex()}")
        print("legacy CDC echo: PASS (AA BB CC)")
        if args.present:
            frame = (
                b'{"v":1,"t":"present","id":"smoke",'
                b'"sum":"Allow smoke test?","opt":["allow","deny"],'
                b'"ttl":30000}\n'
            )
            os.write(fd, frame)
            print("present frame: SENT (Nexting adapter firmware required for answer)")
        return 0
    finally:
        os.close(fd)


if __name__ == "__main__":
    raise SystemExit(main())
