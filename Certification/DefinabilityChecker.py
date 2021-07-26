#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Wed Aug 26 22:55:21 2020

@author: fs
"""

from pysat.solvers import Cadical
import Utils

class DefinabilityChecker:
  
  def __init__(self, formula, existentials):
    self.max_variable = Utils.maxVarIndex(formula + [list(existentials)])
    variables = {abs(l) for clause in formula for l in clause}
    self.renaming = {v: v + self.max_variable for v in variables}
    self.renaming_inverse = {v: k for k, v in self.renaming.items()}
    # Create copy of 'formula' for second part.
    formula_copy = Utils.renameFormula(formula, self.renaming)  
    # Create equality constraints/selectors for all variables.
    self.eq_selector_dict = {v: v + (4 * self.max_variable) for v in variables}
    eq = [c for v in variables
            for c in Utils.equality(v, 
                                    self.renaming[v], 
                                    self.eq_selector_dict[v])]
    self.max_variable = 5 * self.max_variable    
    self.solver = Cadical(bootstrap_with=formula+formula_copy+eq) 
  

  def checkDefinability(self, defining_variables, defined_variable):
      
    eq_enabled = [self.eq_selector_dict[v] for v in defining_variables
                  if v in self.eq_selector_dict]
    assumptions = eq_enabled + [defined_variable,-self.renaming[defined_variable]]
    # Check whether 'defined_variable' is defined.
    satisfiable = self.solver.solve(assumptions)   
    if satisfiable:
      return False, self.getAssignment(defining_variables)
    else:
      return True, None
      
  def getAssignment(self, defining_variables):
    assignment = self.solver.get_values(defining_variables)
    return assignment
    