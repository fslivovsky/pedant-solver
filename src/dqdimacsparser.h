#ifndef PEDANT_DQDIMACS_PARSER_H_
#define PEDANT_DQDIMACS_PARSER_H_

#include <exception>
#include <string>
#include <vector>

#include "dqdimacs.h"

namespace pedant {

class InvalidFileException : public std::exception
{
 public:
  InvalidFileException(const std::string& msg);
  const char * what ();
 
 private:
  std::string msg;
};



class DQDIMACSParser {

 public:
  DQDIMACS parseFormula(const std::string& fname);

 private:

  int processPrefix(std::istream& in);
  int parsePrefix(const std::string& line);
  void parseUniversalBlock(const std::string& line, DQDIMACS& formula);
  void parseExistentialBlock(const std::string& line, DQDIMACS& formula);
  void parseDependencyBlock(const std::string& line, DQDIMACS& formula);
  void parseClause(const std::string& line, DQDIMACS& formula);

  std::vector<int> parseLine(const std::string& line, bool check_is_positive) const;

};

  

}


#endif