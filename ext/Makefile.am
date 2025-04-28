#   Copyright (C) 2019 John Törnblom
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

noinst_LTLIBRARIES = libparson.la libgluecard30.la

libparson_la_CFLAGS = -std=c89 -Wextra -Werror -pedantic -fPIC 
libparson_la_SOURCES = parson.c
libparson_la_LDFLAGS = -static

libgluecard30_la_CXXFLAGS = -std=c++11 -Werror -fPIC 
libgluecard30_la_LDFLAGS = -static
libgluecard30_la_LIBADD = -lstdc++
libgluecard30_la_SOURCES = gluecard30/core/Solver.cc gluecard30/utils/System.cc \
                           gluecard30/ipasir.cc
