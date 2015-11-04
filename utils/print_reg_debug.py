#!/usr/bin/python

import sys
import sys,tty,termios
class _Getch:
    def __call__(self):
            fd = sys.stdin.fileno()
            old_settings = termios.tcgetattr(fd)
            try:
                tty.setraw(sys.stdin.fileno())
                ch = sys.stdin.read(1)
            finally:
                termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
            return ch

f = file(sys.argv[1])

def show_cycle(cycle):
    f.seek(0)
    currText = ""
    print(chr(27) + "[2J")
    for line in f.readlines():
        cycTxt = line.split("]")[0].strip("[")
        thisCyc = int(cycTxt);
        if thisCyc < cycle:
            continue
        if thisCyc > cycle:
            break
        currText += line
    print currText

currCycle = 0
for line in f.readlines():
    cycTxt = line.split("]")[0].strip("[")
    thisCyc = int(cycTxt);
    currCycle = thisCyc
    break

show_cycle(currCycle)
cmdBuffer = ""
getch = _Getch()
while True:
    if sys.stdin.closed:
        break
    c = getch()
    key = ord(c)
    if c == "" or c == "q":
        break
    sys.stdout.write(c)
    sys.stdout.flush()

    if key == 13:
        currCycle = int(cmdBuffer)
        show_cycle(currCycle)
        cmdBuffer = ""
    elif key == 3:
        break
    else:
        cmdBuffer += c
        if cmdBuffer=='\x1b[C':
            #right
            currCycle += 1
            show_cycle(currCycle)
            cmdBuffer = ""
        elif cmdBuffer=='\x1b[D':
            #left
            if currCycle > 1:
                currCycle -= 1
                show_cycle(currCycle)
            cmdBuffer = ""
