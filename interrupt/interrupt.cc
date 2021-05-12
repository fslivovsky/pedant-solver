#include "interrupt.h"

namespace pedant {

int InterruptHandler::signal_received = 0;

void InterruptHandler::interrupt(int signal) {
  signal_received = signal;
}

int InterruptHandler::interrupted(void*) {
  return signal_received;
}

const char* InterruptedException::what() {
  return "SAT call interrupted";
}

}