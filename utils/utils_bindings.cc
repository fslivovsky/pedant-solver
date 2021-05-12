#ifndef PEDANT_UTILS_BINDINGS_H_
#define PEDANT_UTILS_BINDINGS_H_

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "utils.h"

PYBIND11_MODULE(utils_cc, m) {
  m.def("litFromMiniSAT", &pedant::litFromMiniSATLit);
  m.def("miniSAT_literals", &pedant::miniSATLiterals);
  m.def("miniSAT_clauses", &pedant::miniSATClauses);
  m.def("maxVarIndex", &pedant::maxVarIndex);
  m.def("clausalEncodingAND", &pedant::clausalEncodingAND);
  m.def("renameLiteral", pybind11::overload_cast<int, int>(&pedant::renameLiteral));
  m.def("renameClause", &pedant::renameClause);
  m.def("renameFormula", &pedant::renameFormula);
  m.def("createRenaming", &pedant::createRenaming, pybind11::arg("clauses"), pybind11::arg("shared_variables"), pybind11::arg("auxiliary_start") = 0);
  m.def("negate", &pedant::negateFormula, pybind11::arg("clauses"), pybind11::arg("auxiliary_start") = 0);
  m.def("equality", &pedant::clausalEncodingEquality);
}

#endif