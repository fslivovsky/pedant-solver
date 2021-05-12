#ifndef PEDANT_DQDIMACS_PARSER_H_
#define PEDANT_DQDIMACS_PARSER_H_

#include <tuple>
#include <vector>
#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <exception>

#include "solvertypes.h"

namespace pedant {

class InvalidFormatException : public std::exception
{
 public:
  const char * what ();
};

class DQDIMACS_Parser
{
 public: 
  std::tuple<int, std::vector<int>,std::unordered_map<int, std::vector<int>>,std::vector<Clause>> ParseFormula(const std::string& file_name);


 private:
   int ProcessPrefix(const std::string& line);
   void ProcessUniversalBlock(const std::string& line, std::vector<int>& universals);
   void ProcessExistentialBlock(const std::string& line, std::unordered_map<int,std::vector<int>>& dependencies,const std::vector<int>& preceding_universals);
   void ProcessDependencyBlock(const std::string& line, std::unordered_map<int,std::vector<int>>& dependencies);

   void ProcessClause(const std::string& line, std::vector<Clause>& clauses);

};
}
#endif