#include <sstream>

#include "dqdimacsparser.h"

namespace pedant {

InvalidFileException::InvalidFileException(const std::string& msg) : msg(msg) {
}

const char * InvalidFileException::what () {
  return msg.c_str();
}

DQDIMACS DQDIMACSParser::parseFormula(const std::string& fname) {
  std::ifstream input(fname);
  if (!input) {
    throw InvalidFileException("The file " + fname + " could not be opened.");
  }
  DQDIMACS formula;
  int number_of_variables = processPrefix(input);
  std::string line;
  while(std::getline(input,line)) {
    auto position=line.find_first_not_of(" \t\r\n");
    if (position==std::string::npos)
      continue;
    char first_character=line[position];
    switch (first_character) {
      case 'c':
        continue;
        break;
      case 'p':
        throw InvalidFileException("Only one prefix line is allowed.");
        break;
      case 'a':
        line.erase(0,position+1);
        parseUniversalBlock(line, formula);
        break;
      case 'e':
        line.erase(0,position+1);
        parseExistentialBlock(line, formula);
        break;
      case 'd':
        line.erase(0,position+1);
        parseDependencyBlock(line, formula);
        break;
      default:
        parseClause(line, formula);
        break;
    }
  }  
  return formula;
}

int DQDIMACSParser::processPrefix(std::istream& input) {
  std::string line;
  while(std::getline(input,line)) {
    auto position=line.find_first_not_of(" \t\r\n");
    if (position==std::string::npos)
      continue;
    char first_character=line[position];
    switch (first_character) {
      case 'c':
        continue;
        break;
      case 'p':
        line.erase(0,position+1);
        return parsePrefix(line);
        break;
      default:
        throw InvalidFileException("The file has to start with a prefix.");
    }
  }
}


int DQDIMACSParser::parsePrefix(const std::string& line) {
  std::stringstream str_stream(line);
  try {
    std::string str;
    str_stream >> str;
    if (str.compare("cnf")!=0) {
      throw InvalidFileException("The preamble is invalid.");
    }
    str_stream >> str;
    int number_of_variables=std::stoi(str);
    return number_of_variables;
  }  catch(std::invalid_argument& e){
    throw InvalidFileException("The preamble is invalid.");
  }
}

std::vector<int> DQDIMACSParser::parseLine(const std::string& line, bool check_is_positive) const {
  std::stringstream str_stream(line);
  std::string word;
  std::vector<int> result;
  try {
    while (str_stream >> word) {
      int i=std::stoi(word);
      if (i==0) {
        break;
      }
      if (check_is_positive && i<0) {
        throw InvalidFileException("Invalid line - negative int detected: " + line);
      }
      result.push_back(i);
    }
  } catch(std::invalid_argument& e) {
    throw InvalidFileException("Line could not be parsed: " + line);
  }
  return result;
}

void DQDIMACSParser::parseUniversalBlock(const std::string& line, DQDIMACS& formula) {
  std::vector<int> vars = parseLine(line, true);
  formula.addUniversalBlock(vars);
}

void DQDIMACSParser::parseExistentialBlock(const std::string& line, DQDIMACS& formula) {
  std::vector<int> vars = parseLine(line, true);
  formula.addExistentialBlock(vars);
}

void DQDIMACSParser::parseDependencyBlock(const std::string& line, DQDIMACS& formula) {
  std::vector<int> vars = parseLine(line, true);
  if (vars.empty()) {
    throw InvalidFileException("Invalid line - no variable is given: " + line);
  }
  int var = vars[0];
  vars.erase(vars.begin());
  formula.addExplicitDependencies(var, vars);
}

void DQDIMACSParser::parseClause(const std::string& line, DQDIMACS& formula) {
  std::vector<int> lits = parseLine(line, false);
  formula.addClause(lits);
}

}