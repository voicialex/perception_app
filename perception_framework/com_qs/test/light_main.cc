#include <transport/serial_transport.h>

#include <iostream>

int main(int argc, char* argv[]) {
  uint32_t baudrate = 115200;
  std::string dev_path = "/dev/ttyUSB1";
  if (argc == 2) {
    dev_path = argv[1];
  }

  // 1. 打开串口
  SerialTransport serial(dev_path);
  serial.init(baudrate, 1);
  // 2. 设置数据接收回调
  serial.setCallback([](const std::vector<uint8_t>& data) {
    std::cout << "Received: ";
    for (uint8_t byte : data) {
      printf("%02X ", byte);  // 十六进制打印
    }
    std::cout << std::endl;
  });
  serial.start();
  while (1) {
    sleep(1);
  }
  serial.stop();
}