#!/usr/bin/env python

import sys

from serial import Serial

if __name__ == '__main__':
    path = '/dev/ttyUSB0'
    baud = 74880

    if len(sys.argv) > 3:
       print >>sys.stderr, 'usage: %s device baud', sys.argv[0]
       sys.exit(1)
    if len(sys.argv) >= 2:
        path = sys.argv[1]
    if len(sys.argv) == 3:
        baud = int(sys.argv[2])
    
    serial = Serial(port=path, baudrate=baud)
	
    while True:
        print serial.readline(),
