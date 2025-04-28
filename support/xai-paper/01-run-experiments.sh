#!/usr/bin/env bash
#   Copyright (C) 2023 John Törnblom
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

CASES=("ann-thyroid"
       "appendicitis"
       "biodegradation"
       "divorce"
       "ecoli"
       "glass2"
       "ionosphere"
       "pendigits"
       "promoters"
       "segmentation"
       "shuttle"
       "sonar"
       "spambase"
       "texture"
       "threeOf9"
       "twonorm"
       "vowel"
       "wine-recognition"
       "wdbc"
       "wpbc"
       "zoo")

TIMEOUT=3600

mkdir -p $SCRIPTDIR/results

CMDLIST=$(mktemp)
trap 'rm $CMDLIST' EXIT

schedule() {
    echo "$*" >> $CMDLIST
}

for CASE in "${CASES[@]}"; do
    CSV_FILE="$SCRIPTDIR/inputs/$CASE.csv"
    MODEL_FILE="$SCRIPTDIR/inputs/$CASE.json"
    NB_SAMPLES=$(wc -l < $CSV_FILE)
    
    for((IND=0; IND<$NB_SAMPLES;IND++)); do
	schedule $SCRIPTDIR/../../src/vote_explain_minimal \
		 -m $MODEL_FILE \
		 -i $IND \
		 -t $TIMEOUT \
		 -o "$SCRIPTDIR/results/$CASE.explain.minimal.$IND.json" \
		 $CSV_FILE

	schedule $SCRIPTDIR/vote_explain_minimum_mhs.py \
		 -m $MODEL_FILE \
		 -i $IND \
		 -t $TIMEOUT \
		 -s g3 \
		 -o "$SCRIPTDIR/results/$CASE.explain.minimum.MHS.$IND.json" \
		 $CSV_FILE

	schedule $SCRIPTDIR/vote_explain_minimum_marco.py \
		 -m $MODEL_FILE \
		 -i $IND \
		 -t $TIMEOUT \
		 -s gc3 \
		 -b \
		 -o "$SCRIPTDIR/results/$CASE.explain.minimum.m-MARCO-bias.$IND.json" \
		 $CSV_FILE
	
	schedule $SCRIPTDIR/vote_explain_minimum_marco.py \
		 -m $MODEL_FILE \
		 -i $IND \
		 -t $TIMEOUT \
		 -s g3 \
		 -o "$SCRIPTDIR/results/$CASE.explain.minimum.m-MARCO-nocard.$IND.json" \
		 $CSV_FILE

	schedule $SCRIPTDIR/vote_explain_minimum_marco.py \
		 -m $MODEL_FILE \
		 -i $IND \
		 -t $TIMEOUT \
		 -s gc3 \
		 -o "$SCRIPTDIR/results/$CASE.explain.minimum.m-MARCO.$IND.json" \
		 $CSV_FILE

	schedule $SCRIPTDIR/../../src/vote_explain_minimum \
		 -m $MODEL_FILE \
		 -i $IND \
		 -t $TIMEOUT \
		 -a "BB" \
		 -o "$SCRIPTDIR/results/$CASE.explain.minimum.BB.$IND.json" \
		 $CSV_FILE

	schedule $SCRIPTDIR/../../src/vote_explain_forall \
		 -m $MODEL_FILE \
		 -i $IND \
		 -t $TIMEOUT \
		 -o "$SCRIPTDIR/results/$CASE.explain.forall.$IND.json" \
		 $CSV_FILE
    done
done

parallel -a $CMDLIST
