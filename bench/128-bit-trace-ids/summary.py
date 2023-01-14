#!/usr/bin/env python3

import argparse
import math
import statistics


def parse_options():
    parser = argparse.ArgumentParser(
                    prog = 'summary',
                    description = 'Print statistics summary for files')

    parser.add_argument('--cutoff', type=int)

    parser.add_argument('file', nargs='+')

    return parser.parse_args()


options = parse_options()

for path in options.file:
    values = []
    with open(path) as file:
        for line in file:
            line = line.strip()
            if len(line) == 0 or line[0] == '#':
                continue
            value = int(line)
            if options.cutoff is not None and value > options.cutoff:
                continue
            values.append(value)

    print(path)
    for stat in 'mean median mode pstdev stdev'.split():
        print(f'\t{stat}\t', getattr(statistics, stat)(values))

    print('\tmin\t', min(values))
    print('\tmax\t', max(values))
    print('\tcount\t', len(values))
    print('\t√c̅o̅u̅n̅t̅\t', math.sqrt(len(values)))

