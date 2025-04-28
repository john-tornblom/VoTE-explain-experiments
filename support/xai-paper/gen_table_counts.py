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

import vote

from statistics import mean


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


print('%     Dataset    | Input variables   |     Minimal explanations     | Minimum explanations')
print('%                | referenced  total |   min          avg       max | min      avg    max ')
print('%-----------------------------------------------------------------------------------------')
for case in CASES:
    m = vote.Ensemble.from_file(f'{BASEDIR}/inputs/{case}.json')
    usage = m.feature_usage()
    used_vars = len([val for val in usage if val])

    minimal_counts = list()
    minimum_counts = list()
    
    for filename in sorted(glob.glob(f'{BASEDIR}/results/{case}.explain.forall.*json')):
        with open(filename) as f:
            obj = json.load(f)

        if obj['aborted']:
            continue

        minimum_cost  = min([len(e) for e in obj['explanations']])
        minimal_counts.append(len(obj['explanations']))
        minimum_counts.append(len([e for e in obj['explanations']
                                   if len(e) == minimum_cost]))

    if not minimum_counts:
        continue

    print('%16s &       % 3d &   % 3d & %  4d &  %9.1f & % 8d & % 2d &  %4.1f  &  % 4d \\\\' %
          (case, used_vars, m.nb_inputs,
           min(minimal_counts), mean(minimal_counts), max(minimal_counts),
           min(minimum_counts), mean(minimum_counts), max(minimum_counts)))
    
