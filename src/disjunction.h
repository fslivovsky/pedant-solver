#ifndef PEDANT_DISJUNCTION_H_
#define PEDANT_DISJUNCTION_H_

#include <vector>

#include "solvertypes.h"

namespace pedant {

class Disjunction {
 public:
  Disjunction(int& last_used_variable, std::vector<int>& terminals);
  Clause createDisjunct(const Clause& clause);

 protected:
  int& last_used_variable;
  std::vector<int>& terminals;
  int terminal_index;

};

inline Disjunction::Disjunction(int& last_used_variable, std::vector<int>& terminals): last_used_variable(last_used_variable), terminals(terminals) {
  terminal_index = -1; // Indicate that this disjunction has not been initialized.
}

inline Clause Disjunction::createDisjunct(const Clause& clause) {
  Clause disjunct_clause;
  if (terminal_index == -1) {
     // Disjunction not initialized, reserve entry for terminal.
    terminal_index = terminals.size();
    terminals.push_back(0);
  } else {
    disjunct_clause.push_back(terminals[terminal_index]); // This literal is always negative (see following lines).
  }
  // Create new terminal.
  int new_terminal = ++last_used_variable;
  terminals[terminal_index] = -new_terminal;
  // Add literals from clause.
  disjunct_clause.insert(disjunct_clause.end(), clause.begin(), clause.end());
  disjunct_clause.push_back(new_terminal);
  return disjunct_clause;
}

}

#endif // PEDANT_DISJUNCTION_H_