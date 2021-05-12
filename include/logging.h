#ifndef PEDANT_LOGGING_HH
#define PEDANT_LOGGING_HH

#include <iostream>
#include <memory>
#include <vector>

#ifdef NDEBUG
#define DLOG(ARG) if (0) std::cerr
#else
#define DLOG(ARG) Logger::get().setMessageLevel(Loglevel::ARG)
#endif

namespace pedant {

enum class Loglevel: char {trace=1, info=2, error=3};

template<typename T>
std::ostream& operator<<(std::ostream& s, const std::vector<T>& vec) {
  s << "[";
  for (int i = 0; i + 1 < vec.size(); i++) {
    auto& element = vec[i];
    s << element;
    s << ", ";
  }
  if (!vec.empty()) {
    s << vec.back();
  }
  s << "]";
  return s;
}

class StreamSink: public std::ostream, std::streambuf {
public:
  StreamSink(): std::ostream(this), output_level(Loglevel::error) {}

  int overflow(int c)
  {
    conditionalPut(c);
    return 0;
  }

  StreamSink& setMessageLevel(Loglevel level) {
    current_message_level = level;
    return (*this);
  }

  void setOutputLevel(Loglevel level) {
    output_level = level;
  }

  void conditionalPut(char c)
  {
    if (current_message_level >= output_level) {
      std::cerr.put(c);
    }
  }

protected:
  Loglevel output_level, current_message_level;
};

class Logger
{
public:
  static StreamSink& get()
  {
    static StreamSink instance;
    return instance;
  }
private:
  Logger() {}
public:
  Logger(Logger const&) = delete;
  void operator=(Logger const&) = delete;
};

}

#endif // PEDANT_LOGGING_HH