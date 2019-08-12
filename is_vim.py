import sys
import subprocess
import re


def main():
    if len(sys.argv) != 2:
        return 1

    tty = sys.argv[1]
    if tty[0:5] == "/dev/":
        tty = tty[5:]

    binary_output = subprocess.check_output([
        "ps",
        "ax",
        "-o",
        "pid=,ppid=,comm=,state=,tname="
    ])
    output = binary_output.decode("utf-8")
    processes = [re.sub(r"\s+", " ", line.strip()).split(" ") for line in output.split("\n")]

    proc_list = [proc for proc in processes if proc[-1] == tty]

    while len(proc_list) != 0:
        for proc in proc_list:
            if proc[2] in ["vim", "nvim"] and proc[-2] not in ["T", "X", "Z"]:
                return 0

        new_procs = []
        for proc in proc_list:
            for new_proc in processes:
                if new_proc[0] == proc[1]:
                    new_procs.append(new_proc)

        proc_list = new_procs

    return 1


if __name__ == "__main__":
    exit(main())
