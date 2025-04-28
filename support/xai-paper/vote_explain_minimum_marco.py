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

from pysat.solvers import Solver


def utime():
    '''
    Get the accumulated thread time of the current running process, including
    child processes (which is relevant when solvers spawn new processes).
    '''
    return (resource.getrusage(resource.RUSAGE_CHILDREN).ru_utime + 
            resource.getrusage(resource.RUSAGE_SELF).ru_utime)


class ExplainMARCO:
    ensemble = None
    nb_queries = None
    nb_seeds = None
    used_features = None

    def __init__(self, e, solver, biased=False, weights=None):
        self.ensemble = e
        self.nb_queries = 0
        self.nb_seeds = 0
        self.querytime = 0
        self.weights = weights or [1] * self.ensemble.nb_inputs
        self.solver = Solver(solver)
        self.biased = biased
        feature_usage = e.feature_usage()
        self.used_features = set([ind for ind in range(e.nb_inputs)
                                  if feature_usage[ind] > 0])

    def cost(self, seed):
        '''
        Compute the accumulated cost of variables with indices in *seed*.
        '''
        return sum(self.weights[ind] for ind in seed)

    def explore(self):
        '''
        Sample a seed from the solver.
        '''
        # Note: indices in the solver begin with one, while indices in 
        #       explanations begin with zero.
        self.nb_seeds += 1

        if self.biased:
            self.solver.set_phases([(ind+1) for ind in self.used_features])

        if self.solver.solve():
            return set(ind-1 for ind in self.solver.get_model() if ind > 0)

    def block_up(self, seed):
        '''
        Block seeds that are supersets of *seed*.
        '''
        # Note: indices in the solver begin with one, while indices in 
        #       explanations begin with zero.
        c = [-(ind+1) for ind in sorted(seed)]
        self.solver.add_clause(c)

    def block_down(self, seed):
        '''
        Block seeds that are subsets of *seed*.
        '''
        # Note: indices in the solver begin with one, while indices in 
        #       explanations begin with zero.
        c = [(ind+1) for ind in sorted(self.used_features) if ind not in seed]
        self.solver.add_clause(c)
            
    def block_atmost(self, cost):
        '''
        Find a lower bound on the cardinality of seeds that are guaranteed
        to yield a verbosity cost that is less than *cost*, and block them.

        We can accomplish this by enumerating the weights in decending order,
        and growing a set until its cost surpass the given *cost*.
        '''
        if not self.solver.supports_atmost():
            return

        seed = set()
        for ind in reversed(np.argsort(self.weights)):
            if ind in self.used_features:
               if self.cost(seed) <= cost:
                   seed.add(ind)

        k = len(self.used_features - seed)
        self.solver.add_atmost([-(ind + 1)
                                for ind in sorted(self.used_features)], k)

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

    def shrink(self, seed, xvec, mcs_singletons):
        '''
        Shrink *seed* into a minimal unsatisfiable subset (MUS).
        '''
        s = set(seed)
        for ind in sorted(seed):
            # MARCO+ performance boost
            if ind in mcs_singletons:
                continue
            
            s.remove(ind)
            if self.oracle(s, xvec):
                s.add(ind)

        return s

    def grow(self, seed, xvec):
        '''
        Grow *seed* into a maximal satisfiable subset (MSS).
        '''
        if self.biased:
            return seed

        s = set(seed)
        for ind in sorted(self.used_features - seed):
            s.add(ind)
            if not self.oracle(s, xvec):
                s.remove(ind)

        return s

    def minimal(self, xvec):
        to_delete = set()
        for ind in sorted(self.used_features):
            to_delete.add(ind)
            if not self.oracle(to_delete, xvec):
                to_delete.remove(ind)

        return sorted(self.used_features - to_delete)

    def minimum(self, xvec):
        cost = -1
        mss = set()
        mcs_singletons = set()
        
        while True:
            to_delete = self.explore()
            if to_delete is None:
                break

            if self.cost(to_delete) <= cost:
                self.block_down(to_delete)

            elif self.oracle(to_delete, xvec):
                mss = self.grow(to_delete, xvec)
                self.block_down(mss)

                cost = self.cost(mss)
                self.block_atmost(cost)

                mcs = self.used_features - mss
                if len(mcs) == 1:
                    mcs_singletons |= mcs

            else:
                mus = self.shrink(to_delete, xvec, mcs_singletons)
                self.block_up(mus)

        return sorted(self.used_features - mss)

    def forall(self, xvec):
        mcs_singletons = set()
        
        while True:
            to_delete = self.explore()
            if to_delete is None:
                break

            if self.oracle(to_delete, xvec):
                mss = self.grow(to_delete, xvec)
                self.block_down(mss)

                mcs = self.used_features - mss
                if len(mcs) == 1:
                    mcs_singletons |= mcs

                yield sorted(self.used_features - mss)
                
            else:
                mus = self.shrink(to_delete, xvec, mcs_singletons)
                self.block_up(mus)

    
def main():
    p = argparse.ArgumentParser()
    p.add_argument('-m', '--model', metavar='PATH')
    p.add_argument('-i', '--instance', type=int, metavar='NUMBER', default=0)
    p.add_argument('-t', '--timeout', type=int, metavar='SECONDS', default=3600)
    p.add_argument('-o', '--output', metavar='PATH', default='/dev/null')
    p.add_argument('-s', '--solver', metavar='NAME', default='gc3')
    p.add_argument('-b', '--biased', action='store_true')
    p.add_argument('-w', '--index-as-weight', action='store_true')
    p.add_argument('-e', '--forall', action='store_true')
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
    marco = ExplainMARCO(e, args.solver, args.biased, w)
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
        results['queries'] = marco.nb_queries
        results['seeds'] = marco.nb_seeds
        results['querytime'] = marco.querytime
        results['runtime'] = utime() - t

        with open(output, 'w') as f:
            json.dump(results, f)

        sys.exit()

    signal.signal(signal.SIGALRM, on_SIGALARM)
    signal.alarm(args.timeout)

    if args.forall:
        for expl in marco.forall(xvec):
            results['explanations'].append(expl)

    else:
        expl = marco.minimum(xvec)
        results['explanations'].append(expl)

    results['runtime'] = utime() - t
    results['aborted'] = False
    results['queries'] = marco.nb_queries
    results['seeds'] = marco.nb_seeds
    results['querytime'] = marco.querytime

    print(f'explain:model:    {args.model}')
    print(f'explain:dataset:  {args.dataset}')
    print(f'explain:instance: {args.instance}')
    print(f'explain:timeout:  {args.timeout}')
    if args.forall:
        print(f'explain:outcome:  {len(results["explanations"])} explanations')
    else:
        print(f'explain:outcome:  {expl} ({marco.cost(expl)})')
    print(f'explain:queries:  {marco.nb_queries}')
    print(f'explain:runtime:  {results["runtime"]}')
    print(f'explain:seeds:    {results["seeds"]}')

    with open(output, 'w') as f:
        json.dump(results, f)


if __name__ == '__main__':
    main()

