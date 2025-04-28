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

import argparse
import json
import resource
import signal
import sys
import vote

import numpy as np

from pysat.examples.hitman import Hitman


def utime():
    '''
    Get the accumulated thread time of the current running process, including
    child processes (which is relevant when solvers spawn new processes).
    '''
    return (resource.getrusage(resource.RUSAGE_CHILDREN).ru_utime + 
            resource.getrusage(resource.RUSAGE_SELF).ru_utime)


class ExplainMHS:
    ensemble = None
    nb_queries = None
    nb_seeds = None
    used_features = None
    
    def __init__(self, e, solver, weights):
        self.ensemble = e
        self.nb_queries = 0
        self.nb_seeds = 0
        self.querytime = 0
        self.solver = solver
        feature_usage = e.feature_usage()
        self.used_features = frozenset([ind for ind in range(e.nb_inputs)
                                        if feature_usage[ind] > 0])
        self.weights = {ind: weights[ind] for ind in self.used_features}
            
    def cost(self, seed):
        '''
        Compute the accumulated cost of variables with indices in *seed*.
        '''
        return sum(self.weights[ind] for ind in seed)
    
    def oracle(self, to_delete, xvec):
        '''
        Query for the validity of the explanation (with respect to *xvec*)
        where variables with indices in *to_delete* are absent.
        '''
        self.nb_queries += 1
        t = utime()
        expl = self.used_features - to_delete
        res = self.ensemble.explain_is_valid(xvec, expl)
        self.querytime += (utime() - t)
        return res

    def shrink(self, seed, xvec):
        '''
        Shrink *seed* into a minimal unsatisfiable subset (MUS).
        '''
        s = set(seed)
        for ind in sorted(seed):
            s.remove(ind)
            if self.oracle(s, xvec):
                s.add(ind)

        return s
    
    def all_minimal(self, xvec):
        self.nb_queries = 0
        
        with Hitman(bootstrap_with=[self.used_features], weights=self.weights,
                    solver=self.solver, htype='sorted') as hitman:
            # check unit-size explanations
            for ind in self.used_features:
                if self.oracle(self.used_features - {ind}, xvec):
                    yield [ind]
                    hitman.block([ind])

            while True:
                # find a hitting set
                hs = hitman.get()
                if hs is None:
                    break

                self.nb_seeds += 1
                to_delete = self.used_features - set(hs)

                # check validity of explanation
                if self.oracle(to_delete, xvec):
                    yield sorted(hs)
                    hitman.block(hs)

                else:
                    to_delete = self.shrink(to_delete, xvec)
                    hitman.hit(to_delete)

    def minimum(self, xvec):
        return next(self.all_minimal(xvec))


def main():
    p = argparse.ArgumentParser()
    p.add_argument('-m', '--model', metavar='PATH')
    p.add_argument('-i', '--instance', type=int, metavar='NUMBER', default=0)
    p.add_argument('-t', '--timeout', type=int, metavar='SECONDS', default=3600)
    p.add_argument('-o', '--output', metavar='PATH', default='/dev/null')
    p.add_argument('-s', '--solver', metavar='NAME', default='gc3')
    p.add_argument('-w', '--index-as-weight', action='store_true')
    p.add_argument('dataset', metavar='CSV_PATH')
    
    args = p.parse_args()

    e = vote.Ensemble.from_file(args.model)
    d = np.loadtxt(args.dataset, delimiter=',')
    t = utime()
    if args.index_as_weight:
        w = [ind+1 for ind in range(e.nb_inputs)]
    else:
        w = [1] * e.nb_inputs
    xvec = d[args.instance,:].tolist()
    mhs = ExplainMHS(e, args.solver, w)
    output = args.output
    results = {
        'aborted': False,
        'explanations': [],
        'queries': 0,
        'querytime': 0,
        'runtime': 0,
        'seeds': 0,
        'sample': xvec,
    }

    def on_SIGALARM(*args, **kwargs):
        results['aborted'] = True
        results['queries'] = mhs.nb_queries
        results['seeds'] = mhs.nb_seeds
        results['querytime'] = mhs.querytime
        results['runtime'] = utime() - t

        with open(output, 'w') as f:
            json.dump(results, f)

        sys.exit()
    
    signal.signal(signal.SIGALRM, on_SIGALARM)
    signal.alarm(args.timeout)
    
    expl = mhs.minimum(xvec)

    results['aborted'] = False
    results['queries'] = mhs.nb_queries
    results['querytime'] = mhs.querytime
    results['runtime'] = utime() - t
    results['seeds'] = mhs.nb_seeds
    results['explanations'].append(expl)

    print(f'explain:model:    {args.model}')
    print(f'explain:dataset:  {args.dataset}')
    print(f'explain:instance: {args.instance}')
    print(f'explain:timeout:  {args.timeout}')
    print(f'explain:outcome:  {expl} ({mhs.cost(expl)})')
    print(f'explain:queries:  {mhs.nb_queries}')
    print(f'explain:runtime:  {results["runtime"]}')
    print(f'explain:seeds:    {results["seeds"]}')
    
    with open(output, 'w') as f:
        json.dump(results, f)


if __name__ == '__main__':
    main()
    
