#!/usr/bin/python3

# This PNG optimizing script is licensed under the terms
# of the MIT license
# Copyright (c) 2016 Andy Teijelo <github.com/ateijelo>

from subprocess import check_output, STDOUT, CalledProcessError
import os
import sys

def size(f):
    return os.stat(f).st_size

def optimize(f):
    try:
        check_output(["pngnq","-f",f], stderr=STDOUT)
    except CalledProcessError as e:
        print("Error optimizing",f,file=sys.stderr)
        print("  pngnq returned non-zero exit status {}:".format(e.returncode), file=sys.stderr)
        print("  output follows:", file=sys.stderr)
        sys.stderr.flush()
        sys.stderr.buffer.write(e.output)
        sys.stderr.flush()
        return

    nq = f[:-4] + "-nq8.png"
    if size(nq) > size(f):
        os.rename(f, nq)

    crush = f[:-4] + "-crush.png"
    try:
        check_output(["pngcrush",nq,crush], stderr=STDOUT)
    except CalledProcessError as e:
        print("Error optimizing",f,file=sys.stderr)
        print("  pngcrush returned non-zero exit status {}:".format(e.returncode), file=sys.stderr)
        print("  output follows:", file=sys.stderr)
        sys.stderr.flush()
        sys.stderr.buffer.write(e.output)
        sys.stderr.flush()
        return

    if size(crush) > size(nq):
        os.rename(nq, crush)
    os.rename(crush, f)
    os.remove(nq)

if __name__ == "__main__":
    optimize(sys.argv[1])
