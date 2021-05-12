#ifndef PEDANT_INTERRUPT_H_
#define PEDANT_INTERRUPT_H_

#include <iostream>
#include <exception>

namespace pedant {

class InterruptedException: std::exception {
  virtual const char* what();
};

class InterruptHandler {
 public:
  static void interrupt(int signal);
  static int interrupted(void*);

 private:
  static int signal_received;
};

}

#endif // PEDANT_INTERRUPT_H_