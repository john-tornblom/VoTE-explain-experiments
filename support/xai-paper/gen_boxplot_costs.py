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

import seaborn as sns
import pandas as pd

import vote


BASEDIR = os.path.dirname(__file__) or '.'

CASES = ('ann-thyroid',
         'appendicitis',
         'biodegradation',
         'divorce',
         'ecoli',
         'glass2',
         'ionosphere',
         'pendigits',
         'promoters',
         'segmentation',
         'shuttle',
         'sonar',
         'spambase',
         'texture',
         'threeOf9',
         'twonorm',
         'vowel',
         'wdbc',
         'wine-recognition',
         'wpbc',
         'zoo')

rcParams['font.family'] = 'cmr10'
rcParams['font.size'] = 12

data = list()
for case in CASES:
    m = vote.Ensemble.from_file(f'{BASEDIR}/inputs/{case}.json')
    usage = m.feature_usage()
    used_vars = len([val for val in usage if val])

    data.append({'Dataset': case,
                 'Used': used_vars})

    for filename in sorted(glob.glob(f'{BASEDIR}/results/{case}.explain.forall.*json')):
        with open(filename) as f:
            obj = json.load(f)

        if obj['aborted']:
            continue

        minimum = used_vars
        for e in obj['explanations']:
            data.append({'Dataset': case,
                         'Cost': len(e),
                         'Group': 'Minimal'})
            if len(e) < minimum:
                minimum = len(e)

        for e in obj['explanations']:
            if len(e) == minimum:
                data.append({'Dataset': case,
                             'Cost': len(e),
                             'Group': 'Minimum'})


df = pd.DataFrame(data)
ax = sns.boxplot(data=df, palette=(plt.cm.gray(200), plt.cm.gray(125)),
                 fliersize=2, flierprops={'marker':'x'},
                 x='Cost', y='Dataset', hue='Group')

sns.swarmplot(data=df, x='Used', y='Dataset', color=plt.cm.gray(0),
              marker='.', size=14, label='Ref. variables')

handles, labels = ax.get_legend_handles_labels()
ax.legend(handles[:3], labels[:3])

plt.tight_layout()
plt.savefig('figures/boxplot_costs.eps', bbox_inches='tight')
#plt.show()

