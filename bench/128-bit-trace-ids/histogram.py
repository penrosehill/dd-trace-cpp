#!/usr/bin/env python3

import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile


def parse_options():
    parser = argparse.ArgumentParser(
                    prog = 'histogram',
                    description = 'Plot a histogram in gnuplot')

    parser.add_argument('--title', default='Histogram')
    parser.add_argument('--xlabel', default='value')
    parser.add_argument('--xdivisor', type=int, default=1)
    parser.add_argument('--bins', type=int, default=100)

    parser.add_argument('file', nargs='+')

    return parser.parse_args()


options = parse_options()

min_value = None
max_value = None
for path in options.file:
    with open(path) as file:
        for line in file:
            line = line.strip()
            if len(line) == 0 or line[0] == '#':
                continue
            value = int(line)
            if min_value is None or value < min_value:
                min_value = value
            if max_value is None or value > max_value:
                max_value = value

work_dir = Path(tempfile.mkdtemp())
print('Using working directory: ', work_dir)

for path in options.file:
    path = Path(path)
    bins = [0] * options.bins

    with open(path) as file:
        for line in file:
            line = line.strip()
            if len(line) == 0 or line[0] == '#':
                continue
            value = int(line)
            bin_index = int((value - min_value) / (max_value + 1 - min_value) * len(bins))
            bins[bin_index] += 1

    with open(work_dir/path.name, 'w') as file:
        for i, bucket in enumerate(bins):
            x = min_value + i * (max_value - min_value)
            y = bucket
            print(x, y, file=file)

def plot_command(path, xdivisor):
    name = json.dumps(path)
    return f'{name} using ($1/{xdivisor}):2 title {name}'

script = f"""
set title '{options.title}'
set xlabel '{options.xlabel}'
set ylabel 'Frequency'
plot {", ".join(plot_command(path, options.xdivisor) for path in options.file)}
"""

(work_dir/'histogram.plt').write_text(script)

os.chdir(work_dir)
subprocess.run(['gnuplot', '--persist', 'histogram.plt', '-'])
