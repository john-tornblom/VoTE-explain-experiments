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


def load(method):
    for filename in sorted(glob.glob(f'{BASEDIR}/results/*.explain.{method}.*json')):
        with open(filename) as f:
            obj = json.load(f)

        yield obj['queries'], obj['runtime']


Qmarco, Tmarco = zip(*load('minimum.m-MARCO'))
Qmhs  , Tmhs   = zip(*load('minimum.MHS'))

print(f'm-MARCO: {sum(Qmarco)} queries in %.2f hours' % (sum(Tmarco) / 3600.0))
print(f'MHS:     {sum(Qmhs)} queries in %.2f hours' % (sum(Tmhs) / 3600.0))

try:
    plt.style.use('science') # pip3 install SciencePlots
except:
    rcParams['font.family'] = 'cmr10'
    rcParams['font.size'] = 18

fig = plt.figure()
ax = plt.gca()
ax.set_title('Queries per instance')

ax.set_xlabel('m-MARCO')
ax.set_xscale('log');

ax.set_ylabel('MHS')
ax.set_yscale('log');

ax.set_aspect('equal')
ax.axline((1, 1),  (2, 2), color='black', linestyle=':')
ax.scatter(Qmarco, Qmhs, s=1)

fig.tight_layout()
plt.savefig('figures/scatter_minimum_queries.pdf')


fig = plt.figure()
ax = plt.gca()
ax.set_title('Runtime (s) per instance')

ax.set_xlabel('m-MARCO')
ax.set_xscale('log');

ax.set_ylabel('MHS')
ax.set_yscale('log');

ax.set_aspect('equal')
ax.axline((1, 1),  (2, 2), color='black', linestyle=':')
ax.scatter(Tmarco, Tmhs, s=1)

fig.tight_layout()
plt.savefig('figures/scatter_minimum_runtime.pdf')

