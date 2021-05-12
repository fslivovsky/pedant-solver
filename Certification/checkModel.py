#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Fri Sep  4 13:33:51 2020

@author: fs
"""

from pysat.solvers import Cadical

import logging, subprocess, tempfile

def solve2QBF(clauses, universals, existentials):
  with tempfile.NamedTemporaryFile("w") as file:
    file.writelines(toQDIMACS(clauses, universals, existentials))
    file.flush()
    return runCADET(file.name)

def runCADET(filename):
  result = subprocess.run(["cadet", "--qbfcert", filename],
                          stdout=subprocess.PIPE)
  if result.returncode == 20:
    return False, readCertificate(result.stdout)
  else:
    return True, None
  
def readCertificate(output):
  lines = output.splitlines()
  cert_line = lines[3]
  certificate = [int(lit) for lit in cert_line.split()[1:]]
  return certificate

def toQDIMACS(clauses, universals, existentials):
  frontmatter = [preamble(clauses, universals, existentials),
                 quantifierBlock(universals, False),
                 quantifierBlock(existentials, True)]
  clauses = [toDIMACS(clause) for clause in clauses]
  return frontmatter + clauses

def preamble(clauses, universals, existentials):
  return "p cnf {} {}\n".format(max(universals.union(existentials), default=0),
                                len(clauses))

def quantifierBlock(variables, existential=True):
  start = "e " if existential else "a "
  return start + " ".join([str(l) for l in variables] + ["0"]) + "\n"

def toDIMACS(clause):
  return " ".join([str(l) for l in clause]) + " 0\n"

def reduceClause(clause, assignment_set):
  return [l for l in clause if -l not in assignment_set]

def checkModelQBF(clauses, universals, existentials, assumptions=[]):
  assumptions_set = set(assumptions)

  clauses_reduced = [reduceClause(clause, assumptions_set)
                     for clause in clauses
                     if not set(clause).intersection(assumptions_set)]
  logging.debug("Model encoding under assumptions: {}".format(clauses_reduced))
  variables = {abs(l) for clause in clauses_reduced for l in clause}
  assert variables <= set(universals).union(existentials), "free variables"
  
  result, certificate = solve2QBF(clauses_reduced, universals, existentials)
  if not result:
    logging.debug("CADET UNSAT certificate: {}".format(certificate))
    solver = Cadical(bootstrap_with=clauses_reduced)
    assert not solver.solve(certificate), "Certification of UNSAT failed."
  return result, certificate
  
  