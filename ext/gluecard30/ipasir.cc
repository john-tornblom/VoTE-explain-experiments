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

#include "gluecard30/core/Solver.h"


typedef int (ipasir_terminate_cb_t)(void* state);
typedef void (ipasir_learn_cb_t)(void* state, int32_t* clause);


using namespace Gluecard30;


namespace {
  struct ipasir_solver : public Solver {
    vec<Lit> clause;

    Lit
    import(int l) { 
      while(abs(l) > nVars()) {
	newVar();
      }

      return mkLit(Var(abs(l) - 1), (l < 0));
    }
  };
}


extern "C" {

  const char*
  ipasir_signature(void) {
    return "gluecard30";
  }

  ipasir_solver*
  ipasir_init(void) {
    ipasir_solver* s = new ipasir_solver();
    return s;
  }

  void
  ipasir_release(ipasir_solver* s) {
    delete s;
  }

  int
  ipasir_solve(ipasir_solver* s) {
    if(s->solve()) {
      return 10;
    } else {
      return 20;
    }
  }

  void
  ipasir_add(ipasir_solver* s, int32_t l) {
    if(l) {
      s->clause.push(s->import(l));
    } else {
      s->addClause_(s->clause);
      s->clause.clear();
    }
  }

  void
  ipasir_atmost(ipasir_solver* s, int k) {
    s->addAtMost_(s->clause, k);
    s->clause.clear();
  }

  int
  ipasir_val(ipasir_solver* s, int32_t l) {
    lbool lb = gc3l_Undef;

    if(!s->model.size()) {
      return 0;
    }

    lb = s->modelValue(abs(l) - 1);
    if(lb == gc3l_True) {
      return l;
    }
    if(lb == gc3l_False) {
      return -l;
    }
    return 0;
  }

  void
  ipasir_assume(ipasir_solver* s, int32_t l) {
    //TODO
  }

  int
  ipasir_failed(ipasir_solver* s, int32_t l) {
    return 0; //TODO
  }

  void
  ipasir_set_terminate(ipasir_solver* s, void* state,
		       ipasir_terminate_cb_t* cb) {
    // TODO
  }

  void
  ipasir_set_learn(ipasir_solver* s, void* state, int max_length,
		   ipasir_learn_cb_t* cb) {
    // TODO
  }
};

