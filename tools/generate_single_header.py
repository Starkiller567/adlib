#! /usr/bin/env python3

import re
import sys
import os.path

inpath = sys.argv[1]
outpath = sys.argv[2]
outfile = open(outpath, "w")

name, _ = os.path.splitext(os.path.basename(outpath))
outfile.write(f"#ifndef __{name.upper()}_SINGLE_HEADER_INCLUDE__\n")
outfile.write(f"#define __{name.upper()}_SINGLE_HEADER_INCLUDE__\n\n")

outfile.write("#ifndef __AD_LINKAGE\n# define __AD_LINKAGE static\n#endif\n\n")

include_pattern = re.compile(r'\s*#\s*include\s+"(\S*)"\s*')
def process_file(filepath, visited):
    with open(filepath, "r") as f:
        for line in f:
            if match := include_pattern.fullmatch(line):
                header = match.group(1)
                name, ext = os.path.splitext(header)
                assert(ext == ".h")
                outfile.write("\n")
                visit(name, visited)
            else:
                outfile.write(line)

def visit(name, visited = set()):
    if name in visited:
        return
    visited.add(name)
    process_file(os.path.join("include", f"{name}.h"), visited)
    outfile.write(f"#ifndef __{name.upper()}_IMPLEMENTATION_INCLUDE__\n")
    outfile.write(f"#define __{name.upper()}_IMPLEMENTATION_INCLUDE__\n\n")
    process_file(os.path.join("src", f"{name}.c"), visited)
    outfile.write("\n#endif\n\n")

visit(name)
outfile.write("\n#endif\n")
