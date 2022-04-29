#ifndef PEDANT_SOLVER_TYPES_H_
#define PEDANT_SOLVER_TYPES_H_

#include <vector>
#include <tuple>

namespace pedant {

// Alias just for convenience.
using Clause = std::vector<int>;

// Represents an AIG
using Circuit = std::vector<std::tuple<std::vector<int>,int>>;

}

#endif // PEDANT_SOLVER_TYPES_H_