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
    'MHS':             'minimum.MHS',
    'm-MARCO':         'minimum.m-MARCO',
    'BB':              'minimum.BB'
}

def time_series(method):
    method = METHODS[method]
    rt, qt, qn, tn, n = 0, 0, 0, 0, 0
    for filename in sorted(glob.glob(f'{BASEDIR}/results/*.explain.{method}.*.json')):
        n += 1
        
        with open(filename) as f:
            obj = json.load(f)

        if obj['aborted']:
            tn += 1

        rt += obj['runtime']
        qt += obj['querytime']
        qn += obj['queries']
        
    return (rt/3600.0, qt/3600.0, qn, (n-tn) * 100 / float(n))


try:
    plt.style.use('science') # pip3 install SciencePlots
except:
    rcParams['font.family'] = 'cmr10'
    rcParams['font.size'] = 18


print('Method  & Runtime (h) & Query time (h) & Number of queries & Solved instances (%)')
print('---------------------------------------------------------------------------------')
for method in sorted(METHODS):
    rt, qt, qn, frac = time_series(method)
    print(f'{method:7s} &% 12.1f & % 14.1f &  % 16d &               %2.2f' % (rt, qt, qn, frac))

