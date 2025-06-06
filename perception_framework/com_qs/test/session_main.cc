#include <session/session.h>

#include <iostream>

int main(int argc, char* argv[]) {
  Session& session = Session::get_instance();
  session.init();
  session.start();
  while (1) {
    session.control_light_on();
    sleep(1);
    session.control_light_off();
  }
  session.stop();
}