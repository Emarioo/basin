#!/usr/bin/env python3

'''
Requirements:
    gcc
'''

import sys, os, platform, glob, shutil, multiprocessing, threading, subprocess, shlex, datetime
import dataclasses

COLOR_RED = "\033[31m"
COLOR_GREEN = "\033[32m"
COLOR_RESET = "\033[0m"

@dataclasses.dataclass
class BuildConfig:
    # toolchain: str = "gcc"
    debug: bool = True
    optimize: bool = False
    # tracy: bool = False
    bin_dir: str = "bin"
    int_dir: str = "bin/int"
    # build_steps: list[str] = dataclasses.field(default_factory=list) # TODO: 

    # extra options, does not affect the binary
    exe_output: str = "bin/btb"
    # threads: int = multiprocessing.cdpu_count()
    verbose: bool = False
    silent: bool = False

def main():

    config = BuildConfig()

    action_clean: bool = False


    argi = 1
    while argi < len(sys.argv):
        arg = sys.argv[argi]
        argi += 1

        if arg == "clean":
            action_clean = True

    if shutil.which("gcc") is None:
        print(f"{COLOR_RED}ERROR:{COLOR_RESET} Missing GNU Compiler")
        exit(1)

    if action_clean:
        if os.path.exists(config.bin_dir):
            shutil.rmtree(config.bin_dir)
        if os.path.exists(config.int_dir):
            shutil.rmtree(config.int_dir)

    compile_tool(config)

    exit(0)

def compile_tool(config: BuildConfig):
    os.makedirs(config.bin_dir, exist_ok=True)
    os.makedirs(config.int_dir, exist_ok=True)

    ROOT = os.path.dirname(__file__)

    source_files = []
    object_files = []
    for file in glob.glob(f"{ROOT}/src/**/*.c", recursive=True):
        file = file.replace("\\","/")
        
        if file.startswith("_"):
            continue

        file = file.replace("\\", "/")

        # print(file)
        source_files.append(file)
        obj_file = config.int_dir + "/" + os.path.splitext(os.path.basename(file))[0] + ".o"
        obj_file = obj_file.replace("\\", "/")
        object_files.append(obj_file)

    proc = subprocess.run(["git","rev-parse","--short=10","HEAD"],text=True, stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    if proc.returncode != 0:
        if len(proc.stdout) == 0:
            print("build.py: git rev-parse failed")
        print(proc.stdout)
        exit(1)

    commit_file = "bin/int/commit_hash.c"
    commit_hash = proc.stdout.strip()
    build_date = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    with open(commit_file, "w") as f:
        f.write("const char* BASIN_COMPILER_COMMIT = \"" + commit_hash + "\";\n")
        f.write("const char* BASIN_COMPILER_BUILD_DATE = \"" + build_date + "\";\n")
    source_files.append(commit_file)
    object_files.append("bin/int/commit_hash.o")

    @dataclasses.dataclass
    class BuildRuntime:
        # config: BuildConfig
        commands: list[str] = dataclasses.field(default_factory=list)
        next_command_index: int = 0
        failed: bool = False

    runtime = BuildRuntime()
    for src, obj in zip(source_files, object_files):
        # IMPORTANT: DO NOT ADD -Wincompatible-pointer-types. It catches sizeof type mismatch when allocating objects. Explicitly cast to void* to ignore this error.
        CWARNS = ""
        CFLAGS = f"-c -g -I{ROOT}/src -I{ROOT}/include -include {ROOT}/src/basin/pch.h"
        runtime.commands.append(f"gcc {CFLAGS} {CWARNS} -o {obj} {src}")

    def compile_objects(runtime: BuildRuntime):
        while runtime.next_command_index < len(runtime.commands):
            command = runtime.commands[runtime.next_command_index]
            runtime.next_command_index += 1
            
            result = cmd(command)
            if result != 0:
                runtime.next_command_index = len(runtime.commands)
                runtime.failed = True

    threads = []
    for i in range(multiprocessing.cpu_count()):
        t = threading.Thread(target = compile_objects, args = [ runtime ])
        threads.append(t)
        t.start()

    for t in threads:
        t.join()
    
    if runtime.failed:
        exit(1)
    
    PATH_EXE = f"{config.bin_dir}/basin"
    PATH_LIB = f"{config.bin_dir}/libbasin.a"
    
    LDFLAGS = f""
    
    run(f"gcc {LDFLAGS} {' '.join(object_files)} -o {PATH_EXE}")

    run(f"ar rcs {PATH_LIB} {' '.join(object_files)}")


def cmd(c: str, silent: bool = False):
    c = c.replace("\\", "/")
    
    # print(c)
    # os.system(c)
    proc = subprocess.run(shlex.split(c), text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if (proc.returncode != 0 or len(proc.stdout) > 0) and not silent:
        print(proc.stdout, end="")
    return proc.returncode

def run(cmd: str):
    # print(cmd)
    if platform.system() == "Windows":
        res = os.system(cmd)
    else:
        res = os.system(cmd) >> 8
    if res != 0:
        exit(1)

if __name__ == "__main__":
    main()