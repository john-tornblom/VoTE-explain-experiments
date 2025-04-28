#!/usr/bin/env python
# encoding: utf-8
# Copyright (C) 2022 John Törnblom
#
# This file is part of VoTE (Verifier of Tree Ensembles).
#
# VoTE is free software: you can redistribute it and/or modify it under
# the terms of the GNU Lesser General Public License as published by the Free
# Software Foundation, either version 3 of the License, or (at your option) any
# later version.
#
# VoTE is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
# for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with VoTE; see the files COPYING and COPYING.LESSER. If not,
# see <http://www.gnu.org/licenses/>.

import glob
import os
from cffi import FFI

ffibuilder = FFI()

path = os.path.dirname(__file__) or '.'
path = os.path.abspath(path)


sources  = glob.glob(path + '/*.c')
includes = [path + '/../../inc']
defines  = []
objs     = [path + '/../../lib/.libs/libvote.a']
libs     = ['m', 'stdc++']


with open(path + '/../../inc/vote.h') as f:
    vote_h = f.read()

py_interface = '\n'.join([line for line in vote_h.splitlines()
                          if not line.startswith('#')])


ffibuilder.set_source('_vote', vote_h,
                      sources=sources,
                      include_dirs=includes,
                      define_macros=defines,
                      extra_objects=objs,
                      libraries=libs)

ffibuilder.cdef(py_interface + '''
extern "Python" vote_outcome_t _vote_mapping_python_cb(void *, vote_mapping_t*);
extern "Python" bool _vote_explain_python_cb(void *, const bool *, size_t);
extern void free(void *ptr);
''')

if __name__ == "__main__":
    ffibuilder.compile(verbose=True)
    
