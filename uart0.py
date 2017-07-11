#!/usr/bin/env python

from serial import Serial

if __name__ == '__main__':
    serial = Serial(port='/dev/ttyUSB0', baudrate=74880)
	
    while True:
        print serial.readline(),
