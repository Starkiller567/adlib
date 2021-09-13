#! /usr/bin/env python3

import re
import sys
import os.path

inpath = sys.argv[1]
outpath = sys.argv[2]
outfile = open(outpath, "w")

name, _ = os.path.splitext(os.path.basename(outpath))
name = name.upper()
outfile.write(f"#ifndef __{name}_SINGLE_HEADER_INCLUDE__\n")
outfile.write(f"#define __{name}_SINGLE_HEADER_INCLUDE__\n\n")

include_pattern = re.compile(r'\s*#\s*include\s+"(\S*)"\s*')
def process_file(filepath, visited):
    filepath = os.path.realpath(filepath)
    if filepath in visited:
        return
    visited.add(filepath)
    f = open(filepath, "r")
    # TODO just produce a different config.h file with "#define __AD_LINKAGE static" for single headers?
    if filepath.endswith("include/config.h"):
        outfile.write(f.read())
        return
    for line in f:
        if match := include_pattern.fullmatch(line):
            headerpath = os.path.join("include", match.group(1))
            assert(headerpath.endswith(".h"))
            outfile.write("\n")
            process_file(headerpath, visited)
        else:
            outfile.write(line.replace("__AD_LINKAGE", "static"))

process_file(inpath, set())
outfile.write("\n#endif\n")
