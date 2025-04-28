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
import results
import vote
import sys


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




def parse(case, method):
    x, y = 0, 0
    for filename in sorted(glob.glob(f'{BASEDIR}/results/{case}.explain.{method}.*json')):
        with open(filename) as f:
            yield json.load(f)


def print_table(method):
    print('%' + 'Name of dataset | Input variables   |           Runtime          | Number of')
    print('%' + '                | referenced  total |  min       avg        max  | timeouts ')
    print('%' + '----------------------------------------------------------------------------')
    for case in CASES:
        m = vote.Ensemble.from_file(f'{BASEDIR}/inputs/{case}.json')
        usage = m.feature_usage()
        used_vars = len([val for val in usage if val])

        runtime_min = sys.maxsize
        runtime_max = 0
        runtime_all = 0
        timeout_cnt = 0
        cnt = 0
        for obj in parse(case, method):
            cnt += 1
            if obj['aborted']:
                timeout_cnt += 1
            else:
                runtime_min = min(runtime_min, obj['runtime'])
                runtime_max = max(runtime_max, obj['runtime'])
                runtime_all += obj['runtime']

        
        if timeout_cnt < cnt:
            runtime_avg = runtime_all / (cnt - timeout_cnt)
            print('%16s &       % 3d &   % 3d & % 7.2f & % 8.2f & % 8.2f  & % 6d \\\\' %
                  (case, used_vars, m.nb_inputs,
                   runtime_min, runtime_avg, runtime_max,
                   timeout_cnt))
        else:
            print('%16s &       % 3d &   % 3d &      -- &       -- &     --    &  %5d \\\\'
                  % (case, used_vars, m.nb_inputs, timeout_cnt))



print('%Method: m-MARCO')
print_table('minimum.m-MARCO')
print('')

print('%Method: MHS')
print_table('minimum.MHS')
print('')

print('%Method: BB')
print_table('minimum.BB')
print('')

print('%Method: Enumerate')
print_table('forall')
print('')
