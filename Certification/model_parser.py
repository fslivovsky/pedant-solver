#!/usr/bin/env python2
# -*- coding: utf-8 -*-

import re


def parseModelFile(filename): 
  with open(filename) as file:
    return parseModel(file)

def parseModel(in_stream):
  lines = in_stream.readlines()
  [header] = [line for line in lines if line.startswith('p')]
  _, _, nr_vars_string, nr_clauses_string = header.split()
  nof_vars = int(nr_vars_string)
  model_for_variable=None
  model = {}
  clauses = []
  for line in lines:
    if  line.startswith('p') :
      continue
    if  line.startswith('c') :
      x=re.findall("c Model for variable (\d+)", line)
      if len(x)>0:
        model_for_variable=int(x[0])
        if model_for_variable not in model:
          model[model_for_variable] = []
      continue
    if model_for_variable is None:
      return model,clauses
    line_split = line.split()
    if line_split:
      assert line_split[-1] == '0', line
      literals = [int(l_string) for l_string in line_split[:-1]]
      assert all([abs(l) <= nof_vars for l in literals])
      model[model_for_variable].append(literals)
      clauses.append(literals)
  return model,clauses