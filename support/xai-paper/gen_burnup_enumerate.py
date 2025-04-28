#!/usr/bin/env python3
# encoding: utf-8
# Copyright (C) 2022 John Törnblom
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

MARKERS = ['o', 'D', 'x']
COLORS = ['#377eb8', '#ff7f00', '#4daf4a']


def time_serie_of_method(method):
    x, y = 0, 0
    for filename in sorted(glob.glob(f'{BASEDIR}/results/*.explain.{method}.*json')):
        with open(filename) as f:
            obj = json.load(f)

        if not obj['aborted']: y += 1
        x += obj['runtime']

        yield x, y


X_minimal, Y_minimal = zip(*time_serie_of_method('minimal'))
X_minimum, Y_minimum = zip(*time_serie_of_method('minimum.m-MARCO'))
X_forall,  Y_forall  = zip(*time_serie_of_method('forall'))

assert len(X_minimal) == len(X_minimum) == len(X_forall)

# convert x-axis to hours
X_minimal = [x / 3600.0 for x in X_minimal]
X_minimum = [x / 3600.0 for x in X_minimum]
X_forall  = [x / 3600.0 for x in X_forall]

# convert y-axis to fraction of solved instances
Y_minimal = [y / len(Y_minimal) for y in Y_minimal]
Y_minimum = [y / len(Y_minimum) for y in Y_minimum]
Y_forall  = [y / len(Y_forall)  for y in Y_forall]

try:
    plt.style.use('science') # pip3 install SciencePlots
except:
    rcParams['font.family'] = 'cmr10'
    rcParams['font.size'] = 18

plt.plot(X_minimal, Y_minimal, label='Minimal',   color=COLORS[0], marker=MARKERS[0], markevery=1000)
plt.plot(X_minimum, Y_minimum, label='Minimum',   color=COLORS[1], marker=MARKERS[1], markevery=1000)
plt.plot(X_forall,  Y_forall,  label='Enumerate', color=COLORS[2], marker=MARKERS[2], markevery=1000)

plt.ylabel('Fraction of solved instances')
plt.xlabel('Accumulated thread time (h)')
plt.legend()
plt.tight_layout()
plt.savefig('figures/burnup_enumerate.pdf')

#plt.show()
print('MINIMAL:')
print('  solved: %0.2f '      % Y_minimal[-1])
print('  time:   %0.2f hours' % X_minimal[-1])

print('MINIMUM:')
print('  solved: %0.2f '      % Y_minimum[-1])
print('  time:   %0.2f hours' % X_minimum[-1])


print('ENUMERATE:')
print('  solved: %0.2f '      % Y_forall[-1])
print('  time:   %0.2f hours' % X_forall[-1])
