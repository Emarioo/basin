#!/usr/bin/env python3

'''
Requirements:
    gcc
'''

import sys, os, platform, glob

def main():
    
    argi = 1
    while argi < len(sys.argv):
        arg = sys.argv[argi]
        argi += 1

    compile_tool()

def compile_tool():
    os.makedirs("bin", exist_ok=True)

    ROOT = os.path.dirname(__file__)
    
    SRC = []
    for file in glob.glob(f"{ROOT}/src/**/*.c", recursive=True):
        file = file.replace("\\","/")
        
        if file.startswith("_"):
            continue

        # print(file)
        SRC.append(file)

    SRC = " ".join(SRC)
    
    OUT = "bin/basin"
    
    # IMPORTANT: DO NOT ADD -Wincompatible-pointer-types. It catches sizeof type mismatch when allocating objects. Explicitly cast to void* to ignore this error.
    CWARN = ""
    CFLAGS = f"-g -I{ROOT}/src -I{ROOT}/include -include {ROOT}/src/basin/pch.h"
    run(f"gcc {CFLAGS} {SRC} -o {OUT}")


def run(cmd):
    # print(cmd)
    if platform.system() == "Windows":
        res = os.system(cmd)
    else:
        res = os.system(cmd) >> 8
    if res != 0:
        exit(1)

if __name__ == "__main__":
    main()