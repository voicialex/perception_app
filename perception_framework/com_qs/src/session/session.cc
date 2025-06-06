#include <session/session.h>

#pragma pack(push, 1)
struct LightProtocol {
  uint8_t sync;
  uint8_t addr;
  uint8_t cmd1;
  uint8_t cmd2;
  uint8_t data1;
  uint8_t data2;
  uint8_t crc;
};
#pragma pack(pop)

uint8_t py_crc_8_s(const uint8_t* di, uint32_t len) {
  uint8_t crc_poly = 0x07;  // X^8+X^2+X^1+1 total 8 effective bits without X^8. Computed total data shall be
                            // compensated 8-bit '0' before CRC computing.

  uint32_t clen = len + 1;
  uint8_t cdata;
  uint8_t data_t = di[0];  // CRC register

  for (uint32_t i = 1; i < clen; i++) {
    cdata = di[i];
    if (i == clen - 1) {
      cdata = 0;
    }
    for (uint8_t j = 0; j <= 7; j++) {
      if (data_t & 0x80)
        data_t = ((data_t << 1) | ((cdata >> (7 - j)) & 0x01)) ^ crc_poly;
      else
        data_t = ((data_t << 1) | ((cdata >> (7 - j)) & 0x01));
    }
  }
  return data_t;
}

void Session::init() {
  init_light_com();
  init_control_com();
}

void Session::init_light_com() {
  uint32_t baudrate = 115200;
  std::string dev_path = "/dev/ttyUSB0";
  light_com_ = std::make_shared<SerialTransport>(dev_path);
  light_com_->init(baudrate, 3);
}

void Session::control_light_on() {
  std::cout << "send light on msg" << std::endl;
  std::vector<uint8_t> data;
  LightProtocol content = {0xff, 0x1, 0, 9, 0, 1, 0};
  content.crc = py_crc_8_s(reinterpret_cast<const uint8_t*>(&content), 6);

  // 修正1: 将content的内容复制到vector中
  data.assign(reinterpret_cast<const uint8_t*>(&content), reinterpret_cast<const uint8_t*>(&content) + sizeof(content));

  if (light_com_) {
    // 修正2: 发送vector的数据而不是vector本身的地址
    light_com_->async_send(data);
  }
}

void Session::control_light_off() {
  std::cout << "send light off msg" << std::endl;
  std::vector<uint8_t> data;
  LightProtocol content = {0xff, 0x1, 0, 0xB, 0, 1, 0};
  content.crc = py_crc_8_s(reinterpret_cast<const uint8_t*>(&content), 6);

  // 修正1: 将content的内容复制到vector中
  data.assign(reinterpret_cast<const uint8_t*>(&content), reinterpret_cast<const uint8_t*>(&content) + sizeof(content));

  if (light_com_) {
    // 修正2: 发送vector的数据而不是vector本身的地址
    light_com_->async_send(data);
  }
}

void Session::init_control_com() {}

void Session::start() { light_com_->start(); }

void Session::stop() { light_com_->stop(); }
