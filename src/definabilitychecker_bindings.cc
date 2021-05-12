#include <vector>
#include <unordered_map>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "solvertypes.h"
#include "definabilitychecker.h"

namespace py = pybind11;

PYBIND11_MODULE(definabilitychecker_python, m) {
    py::class_<pedant::DefinabilityChecker>(m, "DefinabilityCheckerCC")
        .def(py::init<std::vector<pedant::Clause>&, std::vector<int>&, bool>())
        .def("checkDefinability", &pedant::DefinabilityChecker::checkDefinability)
        .def("addSharedVariable", &pedant::DefinabilityChecker::addSharedVariable)
        .def("addClause", &pedant::DefinabilityChecker::addClause)
        .def("maxVariable", &pedant::DefinabilityChecker::getMaxVariable);
}