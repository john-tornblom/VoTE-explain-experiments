#!/usr/bin/env python3
# encoding: utf-8
# Copyright (C) 2023 John Törnblom
#
# This file is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; see the file COPYING. If not see
# <http://www.gnu.org/licenses/>.
'''
'''
import glob
import json
import os

import matplotlib.pyplot as plt
from matplotlib import rcParams


BASEDIR = os.path.dirname(__file__) or '.'

METHODS = {
    'm-MARCO':         'minimum.m-MARCO',
    'm-MARCO(bias)':   'minimum.m-MARCO-bias',
    'm-MARCO(nocard)': 'minimum.m-MARCO-nocard',
}


MARKERS = ['o', 'D', 'x']
COLORS = ['#377eb8', '#ff7f00', '#4daf4a']


def time_series(method):
    method = METHODS[method]
    x, y = 0, 0
    for filename in sorted(glob.glob(f'{BASEDIR}/results/*.explain.{method}.*.json')):
        with open(filename) as f:
            obj = json.load(f)

        if not obj['aborted']:
            y += 1

        x += obj['runtime']
            
        yield x, y


try:
    import scienceplots  # pip3 install SciencePlots
    plt.style.use('science')
except:
    rcParams['font.family'] = 'cmr10'
    rcParams['font.size'] = 18

fig = plt.figure()
for lbl, marker, color in zip(sorted(METHODS), MARKERS, COLORS):
    X, Y = zip(*time_series(lbl))
    X = [x / 3600.0 for x in X]
    Y = [y / len(Y) for y in Y]
    plt.plot(X, Y, label=lbl, marker=marker,markevery=1000, color=color)
    
    print(f'{lbl}:')
    print('  solved: %0.2f %% ' % (Y[-1] * 100))
    print('  time:   %0.2f hours' % X[-1])

plt.title('')
plt.ylabel('Fraction of solved instances')
plt.xlabel('Accumulated thread time (h)')
plt.legend()
plt.tight_layout()
plt.savefig('figures/burnup_minimum.pdf')

