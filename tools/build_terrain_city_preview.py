#!/usr/bin/env python3
"""Link the screenshot fixture against a completed Linux Ninja Debug build."""
from pathlib import Path
import argparse
import shlex
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build-dir', default='build')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
build = (root / args.build_dir).resolve()
commands = subprocess.check_output(['ninja', '-t', 'commands', 'mmapper'], cwd=build, text=True).splitlines()
compile_command = next(c for c in commands if ' -c ' in c and c.endswith('/src/main.cpp'))
compile_args = shlex.split(compile_command)
compile_args[compile_args.index('-o') + 1] = str(build / 'city-preview.o')
compile_args[compile_args.index('-c') + 1] = str(root / 'tools/terrain_city_preview.cpp')
compile_args[compile_args.index('-MF') + 1] = str(build / 'city-preview.o.d')
subprocess.run(compile_args, cwd=build, check=True)
link = next(c for c in commands if ' -o src/mmapper ' in c).removeprefix(': && ').removesuffix(' && :')
link_args = shlex.split(link)
link_args[link_args.index('-o') + 1] = str(build / 'city-preview')
link_args = [str(build / 'city-preview.o') if a == 'src/CMakeFiles/mmapper.dir/main.cpp.o' else a for a in link_args]
subprocess.run(link_args, cwd=build, check=True)
print(build / 'city-preview')
