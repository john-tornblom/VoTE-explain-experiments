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
Generate scripts that can be invoked from SLURM that, based on
previous runs of the experiments, takes rouhgly 12 hours to run
on a single CPU core.
'''

import glob
import json
import os
import string


JOB_TIMEOUT = 60*60*4

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


CMDS = {
    'minimum.BB': string.Template('''../../src/vote_explain_minimum \\
      -m $MODEL_FILE \\
      -i $IND \\
      -t $TIMEOUT \\
      -a "BB" \\
      -o          results/$CASE.explain.minimum.BB.$IND.json \\
      $CSV_FILE > results/$CASE.explain.minimum.BB.$IND.log
      '''),
    
    'minimum.MHS': string.Template('''./vote_explain_minimum_mhs.py \\
      -m $MODEL_FILE \\
      -i $IND \\
      -t $TIMEOUT \\
      -s g3 \\
      -o          results/$CASE.explain.minimum.MHS.$IND.json \\
      $CSV_FILE > results/$CASE.explain.minimum.MHS.$IND.log
      '''),

    'minimum.m-MARCO': string.Template('''./vote_explain_minimum_marco.py \\
      -m $MODEL_FILE \\
      -i $IND \\
      -t $TIMEOUT \\
      -s gc3 \\
      -o          results/$CASE.explain.minimum.m-MARCO.$IND.json \\
      $CSV_FILE > results/$CASE.explain.minimum.m-MARCO.$IND.log
      '''),

    'minimum.m-MARCO-bias': string.Template('''./vote_explain_minimum_marco.py \\
      -m $MODEL_FILE \\
      -i $IND \\
      -t $TIMEOUT \\
      -s gc3 \\
      -b \\
      -o          results/$CASE.explain.minimum.m-MARCO-bias.$IND.json \\
      $CSV_FILE > results/$CASE.explain.minimum.m-MARCO-bias.$IND.log
      '''),

    'minimum.m-MARCO-nocard': string.Template('''./vote_explain_minimum_marco.py \\
      -m $MODEL_FILE \\
      -i $IND \\
      -t $TIMEOUT \\
      -s g3 \\
      -b \\
      -o          results/$CASE.explain.minimum.m-MARCO-nocard.$IND.json \\
      $CSV_FILE > results/$CASE.explain.minimum.m-MARCO-nocard.$IND.log
      '''),

    'forall': string.Template('''../../src/vote_explain_forall \\
      -m $MODEL_FILE \\
      -i $IND \\
      -t $TIMEOUT \\
      -o results/$CASE.explain.forall.$IND.json \\
      $CSV_FILE > results/$CASE.explain.forall.$IND.log
      ''')
}


def mk_cmd(cmd_name, case, instance):
    return CMDS[cmd_name].substitute(MODEL_FILE=f'inputs/{case}.json',
                                     IND=instance,
                                     TIMEOUT=3600,
                                     CASE=case,
                                     CSV_FILE=f'inputs/{case}.csv')


def mk_job(cmd_name, cmds, jobid):
    filename = f'jobs/{cmd_name}.{jobid}.sh'
    print(filename)
    with open(filename, 'w') as f:
        f.write('#!/usr/bin/env bash\n')
        f.write(cmds)


def mk_pattern(method, case):
    return f'{BASEDIR}/results/{case}.explain.{method}.*.json'


def main():
    for name in CMDS:
        commands = list()
        for case in CASES:
            pattern = mk_pattern(name, case)
            for filename in sorted(glob.glob(pattern)):
                #print(filename)
                instance = os.path.basename(filename).split('.')[-2]
                cmd = mk_cmd(name, case, instance)
                with open(filename) as f:
                    obj = json.load(f)
                time = obj['runtime'] * 1.1
                commands.append((cmd, time))

        s = ''
        t = j = 0
        for cmd, time in commands:
            if time + t > JOB_TIMEOUT:
                mk_job(name, s, j)
                s = ''
                t = 0
                j += 1

            s += '\n\n# runtime probobly less than %0.2fs\n' % time
            s += cmd
            t += time
        
        if s:
            mk_job(name, s, j)


if __name__ == '__main__':
    main()
