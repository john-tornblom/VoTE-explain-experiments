#!/usr/bin/env python3

import glob
import json
import os

BASEDIR = os.path.dirname(__file__) or '.'
CASES = dict()

METHODS = {
    'MHS':             'minimum.MHS',
    'm-MARCO':         'minimum.m-MARCO',
    'm-MARCO(nocard)': 'minimum.m-MARCO-nocard',
    'm-MARCO(bias)':   'minimum.m-MARCO-bias',
    'BB':              'minimum.BB'
}


for case in sorted(glob.glob(f'{BASEDIR}/inputs/*.csv')):
    with open(case) as f:
        nb_samples = len(f.read().strip().split('\n'))

    name = os.path.basename(case)[:-4]
    CASES[name] = nb_samples


def check_minimal(filename):
    try:
        with open(filename) as f:
            obj = json.load(f)

        assert(len(obj['explanations']) == 1)
    except Exception as ex:
        print('%s: %s' % (os.path.basename(filename), ex))

        
def check_minimum(filename):
    try:
        with open(filename) as f:
            obj = json.load(f)

        if not obj['aborted']:
            assert(len(obj['explanations']) == 1)
    except Exception as ex:
        print('%s: %s' % (os.path.basename(filename), ex))

        
def check_forall(filename):
    try:
        with open(filename) as f:
            obj = json.load(f)

        assert(len(obj['explanations']) >= 1)
            
    except Exception as ex:
        print('%s: %s' % (os.path.basename(filename), type(ex)))


def cmp_minimum(filename1, filename2):
    try:
        with open(filename1) as f:
            obj1 = json.load(f)

        with open(filename2) as f:
            obj2 = json.load(f)
            
        if obj1['aborted'] or obj2['aborted']:
            return

        assert(len(obj1['explanations']) == len(obj2['explanations']))
        
    except Exception as ex:
        print('%s vs %s: %s' % (os.path.basename(filename1), os.path.basename(filename2), ex))


for case, nb_samples in sorted(CASES.items()):
    for ind in range(nb_samples):
        check_minimal(f'{BASEDIR}/results/{case}.explain.minimal.{ind}.json')
        check_forall(f'{BASEDIR}/results/{case}.explain.forall.{ind}.json')

        for method in METHODS.values():
            check_minimum(f'{BASEDIR}/results/{case}.explain.{method}.{ind}.json')

            for method2 in METHODS.values():
                if method == method2:
                    continue
                
                cmp_minimum(f'{BASEDIR}/results/{case}.explain.{method}.{ind}.json',
                            f'{BASEDIR}/results/{case}.explain.{method2}.{ind}.json')
                

