#!/usr/bin/env python3
import sys

for line in sys.stdin:
    line = line.strip()
    dwords = [line[i:i+8] for i in range(0, len(line), 8)]
    print(''.join(['{}`'.format(int(dword, 16)) for dword in dwords]))
