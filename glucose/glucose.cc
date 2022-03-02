#include "glucose-ipasir.h"


namespace pedant {

void GlucoseSolver::addClause(const Clause& clause) {
  Glucose::vec<Glucose::Lit> glucose_clause;
  for (int l:clause) {
    if (abs(l)>max_var) {
      max_var=abs(l);
    }
    glucose_clause.push(MakeGlucoseLiteral(l));
  }
  while (solver.nVars()<=max_var) {
    solver.newVar();
  }
  solver.addClause(glucose_clause);
}



int GlucoseSolver::solve(const std::vector<int>& assms) {
  Glucose::vec<Glucose::Lit> glucose_assumptions;
  for (int a:assms) {
    if (abs(a)>max_var) {
      max_var=abs(a);
    }
    glucose_assumptions.push(MakeGlucoseLiteral(a));
  }
  for (int a:assumptions) {
    if (abs(a)>max_var) {
      max_var=abs(a);
    }
    glucose_assumptions.push(MakeGlucoseLiteral(a));
  }
  assumptions.clear();
  while (solver.nVars()<=max_var) {
    solver.newVar();
  }
  bool Sat=solver.solve(glucose_assumptions);
  if (Sat) {
    return 10;
  } else {
    return 20;
  }
}

int GlucoseSolver::solve() {
  Glucose::vec<Glucose::Lit> glucose_assumptions;
  for (int a:assumptions) {
    if (abs(a)>max_var) {
      max_var=abs(a);
    }
    glucose_assumptions.push(MakeGlucoseLiteral(a));
  }
  assumptions.clear();
  while (solver.nVars()<=max_var) {
    solver.newVar();
  }
  bool Sat=solver.solve(glucose_assumptions);

  if (Sat) {
    return 10;
  } else {
    return 20;
  }
}

int GlucoseSolver::solve(int conflict_limit) {
  Glucose::vec<Glucose::Lit> glucose_assumptions;
  for (int a:assumptions) {
    if (abs(a)>max_var) {
      max_var=abs(a);
    }
    glucose_assumptions.push(MakeGlucoseLiteral(a));
  }
  assumptions.clear();
  while (solver.nVars()<=max_var) {
    solver.newVar();
  }
  solver.setConfBudget(conflict_limit);
  Glucose::lbool Sat=solver.solveLimited(glucose_assumptions);

  //Macros with the name l_True / l_False are also used in other parts of the program (e.g. in Minisat).
  //In order to clearly distinct those macros we renamed them instead of undefining them.
  if (Sat == l_True_Glucose) {
    return 10;
  } else if (Sat == l_False_Glucose) {
    return 20;
  } else {
    return 0;
  }
}

std::vector<int> GlucoseSolver::getModel() {
  const Glucose::vec<Glucose::lbool>& glucose_model=solver.model;
  std::vector<int> values;
  for (int var=1;var<glucose_model.size();var++) {
    int lit = var * ((glucose_model[var] == l_True_Glucose) ? 1 : -1);
    values.push_back(lit);
  }
  return values;
}

int GlucoseSolver::val(int var) {
  const Glucose::vec<Glucose::lbool>& glucose_model=solver.model;
  return var * ((glucose_model[var] == l_True_Glucose) ? 1 : -1);
}

std::vector<int> GlucoseSolver::getFailed(const std::vector<int>& assumptions) {
  const Glucose::vec<Glucose::Lit>& conflict=solver.conflict;
  auto assumptions_copy = assumptions;
  std::sort(assumptions_copy.begin(), assumptions_copy.end());
  std::vector<int> failed;
  for (int i=0;i<conflict.size();i++) {
    int l = Glucose::var(conflict[i]) * (Glucose::sign(conflict[i])? 1 : -1);
    if (std::binary_search(assumptions_copy.begin(),assumptions_copy.end(),l)) {
      failed.push_back(l);
    }
  }
  return failed;
}



std::vector<int> GlucoseSolver::getValues(const std::vector<int>& variables) {
  std::vector<int> values;
  for (int v:variables) {
    if (v>solver.nVars()) {
      continue;
    }
    Glucose::lbool value=solver.modelValue(MakeGlucoseVariable(v));
    if (value==l_True_Glucose) {
      values.push_back(v);
    } else if (value==l_False_Glucose) {
      values.push_back(-v);
    }
  }
  return values;
}


}