#!/usr/bin/env bash
#   Copyright (C) 2022 John Törnblom
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

SCRIPTDIR="${BASH_SOURCE[0]}"
SCRIPTDIR="$(dirname "${SCRIPTDIR}")"

export PYTHONPATH=$SCRIPTDIR/../../bindings/python

echo "Generating figures, this require 32GiB of RAM and may take a while..."
sleep 5
echo ""

echo "MINIMUM TABLE"
python3 $SCRIPTDIR/gen_table_minimum.py > figures/table_minimum.tex

echo "MINIMUM BURNUP CHARTS"
python3 $SCRIPTDIR/gen_burnup_minimum.py > figures/gen_burnup_minimum.log

echo "MINIMUM SCATTER PLOTS"
python3 $SCRIPTDIR/gen_scatter_minimum.py > figures/gen_scatter_minimum.log

echo "ENUMERATE BURNUP CHART"
python3 $SCRIPTDIR/gen_burnup_enumerate.py > figures/gen_burnup_enumerate.log

echo "COST OF EXPLANATIONS BOXPLOT"
python3 $SCRIPTDIR/gen_boxplot_costs.py > figures/gen_boxplot_costs.log
gs -sDEVICE=pdfwrite -dPDFSETTINGS=/printer -dEPSCrop -o figures/boxplot_costs.pdf figures/boxplot_costs.eps > /dev/null
gs -sDEVICE=jpeg -dJPEGQ=100 -r600 -dEPSCrop -o figures/boxplot_costs.jpg figures/boxplot_costs.eps > /dev/null

echo "NUMBER OF EXPLANATIONS TABLE"
python3 $SCRIPTDIR/gen_table_counts.py > figures/table_counts.tex

# Appendix
echo "DETAILED RUNTIME TABLES"
python3 $SCRIPTDIR/gen_table_perf.py > figures/tables_perf.tex

echo "DONE"
