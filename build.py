#!/usr/bin/env python3

'''
Requirements:
    gcc
'''

import sys, os, platform

def main():
    
    argi = 1
    while argi < len(sys.argv):
        arg = sys.argv[argi]
        argi += 1

    compile_tool()

def compile_tool():
    os.makedirs("bin", exist_ok=True)

    ROOT = os.path.dirname(__file__)
    SRC = f"{ROOT}/src/basin/main.c"
    OUT = "bin/basin"
    CFLAGS = f"-g -I{ROOT}/src/basin -I{ROOT}/include -include {ROOT}/src/basin/pch.h"
    run(f"gcc {CFLAGS} {SRC} -o {OUT}")

def run(cmd):
    print(cmd)
    if platform.system() == "Windows":
        res = os.system(cmd)
    else:
        res = os.system(cmd) >> 8
    if res != 0:
        exit(1)

if __name__ == "__main__":
    main()