#include "cadical.h"

namespace pedant {

CadicalSolver::CadicalTerminator CadicalSolver::terminator;

CadicalSolver::CadicalSolver() {
  solver.connect_terminator(&terminator);
}

CadicalSolver::~CadicalSolver() {
  solver.disconnect_terminator();
}

bool CadicalSolver::CadicalTerminator::terminate() {
  return InterruptHandler::interrupted(nullptr);
}

}