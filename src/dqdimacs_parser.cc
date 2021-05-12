#include "dqdimacs_parser.h"

namespace pedant {

const char * InvalidFormatException::what () {
    return "The given file contains an invalid DQDIMACS";
}

std::tuple<int, std::vector<int>,std::unordered_map<int, std::vector<int>>,std::vector<Clause>> DQDIMACS_Parser::ParseFormula(const std::string& file_name)
{
  std::ifstream input(file_name);
  if (input) {
    std::vector<int> universal_variables;
    std::unordered_map<int,std::vector<int>> dependency_dict;
    std::vector<Clause> clauses;
    std::string line;
    int number_of_variables;
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
          number_of_variables=ProcessPrefix(line);
          break;
        case 'a':
          line.erase(0,position+1);
          ProcessUniversalBlock(line,universal_variables);
          break;
        case 'e':
          line.erase(0,position+1);
          ProcessExistentialBlock(line,dependency_dict,universal_variables);
          break;
        case 'd':
          line.erase(0,position+1);
          ProcessDependencyBlock(line,dependency_dict);
          break;
        default:
          ProcessClause(line,clauses);
          break;
      }
    }  
    return std::make_tuple(number_of_variables,universal_variables,dependency_dict,clauses); 
    input.close();
  } else {
    throw InvalidFormatException();
  }  
}

int DQDIMACS_Parser::ProcessPrefix(const std::string& line)
{
  std::stringstream str_stream(line);
  try {
    std::string str;
    str_stream >> str;
    if (str.compare("cnf")!=0) {
      throw InvalidFormatException();
    }
    str_stream >> str;
    int number_of_variables=std::stoi(str);
    return number_of_variables;
    
  }  catch(std::invalid_argument& e){
    throw InvalidFormatException();
  }
  
}

void DQDIMACS_Parser::ProcessUniversalBlock(const std::string& line,
                                            std::vector<int>& universals) {
  std::stringstream str_stream(line);
  std::string word;
  try {
    while (str_stream >> word) {
      int i=std::stoi(word);
      if (i==0) {
        return;
      } else if(i<0) {
        throw InvalidFormatException();
      } else {
        universals.push_back(i);    
      }
    }
  } catch(std::invalid_argument& e) {
    throw InvalidFormatException();
  }
}

void DQDIMACS_Parser::ProcessExistentialBlock(const std::string& line, 
    std::unordered_map<int,std::vector<int>>& dependency_dict,
    const std::vector<int>& preceding_universals) {
  std::stringstream str_stream(line);
  std::string word;
  try {
    while (str_stream >> word) {
      int i=std::stoi(word);
      if (i==0) {
        return;
      } else if (i<0) {
        throw InvalidFormatException();
      } else {
        std::vector<int> dependencies(preceding_universals);
        dependency_dict.insert(std::unordered_map<int,std::vector<int>>::value_type(i,dependencies));  
      }
    }
  } catch(std::invalid_argument& e) {
    throw InvalidFormatException();
  }
  

}

void DQDIMACS_Parser::ProcessDependencyBlock(const std::string& line,
    std::unordered_map<int,std::vector<int>>& dependency_dict) {
  std::stringstream str_stream(line);
  std::string word;
  try {
    if (!(str_stream >> word)) {
      throw InvalidFormatException();
    }
    int variable=std::stoi(word);
    if (variable<=0) {
      throw InvalidFormatException();
    }
    std::vector<int> dependencies;
    while (str_stream >> word) {
      int i=std::stoi(word);
      if (i==0) {
        break;
      } else if (i<0) {
        throw InvalidFormatException();
      } else {
        dependencies.push_back(i);
      }
    }
    dependency_dict.insert(std::unordered_map<int,std::vector<int>>::value_type(variable,dependencies));
  } catch(std::invalid_argument& e) {
    throw InvalidFormatException();
  }
}

void DQDIMACS_Parser::ProcessClause(const std::string& line,
                                    std::vector<Clause>& clauses) {
  std::stringstream str_stream(line);
  std::string word;
  Clause c;
  try {
    while (str_stream >> word) {
      int i=std::stoi(word);
      if (i==0) {
        break;
      } else {
        c.push_back(i);     
      }
    }
    clauses.push_back(c);  
  } catch(std::invalid_argument& e) {
    throw InvalidFormatException();
  }

}



}