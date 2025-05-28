#include <transport/serial_transport.h>

#include <iostream>

class Session {
 public:
  void init();
  void control_light_on();
  void control_light_off();
  void start();
  void stop();
  static Session& get_instance() {
    static Session instance;
    return instance;
  }

 private:
  Session() = default;
  Session(const Session&) = delete;
  Session(Session&&) noexcept = default;
  Session& operator=(const Session&) = delete;
  Session& operator=(Session&&) noexcept = default;
  void init_light_com();
  void init_control_com();
  std::shared_ptr<SerialTransport> light_com_;
};
