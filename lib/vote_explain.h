/* Copyright (C) 2023 John Törnblom

   This file is part of VoTE (Verifier of Tree Ensembles).

VoTE is free software: you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the Free
Software Foundation, either version 3 of the License, or (at your option) any
later version.

VoTE is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
for more details.

You should have received a copy of the GNU Lesser General Public
License along with VoTE; see the files COPYING and COPYING.LESSER. If not,
see <http://www.gnu.org/licenses/>.  */

#ifndef VOTE_EXPLAIN_H
#define VOTE_EXPLAIN_H

#include "vote.h"


/**
 * Check that all inputs map to a given label.
 **/
bool vote_explain_check_label(const vote_ensemble_t *e,
			      const vote_bound_t *inputs,
			      size_t label,
			      vote_bound_t *coex);


#endif //VOTE_EXPLAIN_H
