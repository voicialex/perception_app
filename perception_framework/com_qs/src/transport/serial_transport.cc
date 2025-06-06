#include "transport/serial_transport.h"

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <system_error>
#include <thread>

SerialTransport::SerialTransport(const std::string& port) : port_(port) {}

SerialTransport::~SerialTransport() { stop(); }

// retry_times < 0 无限retry
bool SerialTransport::init(uint32_t baudrate, int retry_times) {
  while (retry_times < 0 || retry_times-- > 0) {
    fd_ = open(port_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
      std::cout << "open " << port_ << " failed" << std::endl;
      std::this_thread::sleep_for(std::chrono::seconds(1));
      continue;
    }
    // 配置串口参数
    if (!configure(baudrate)) {
      std::cout << "configure serial failed and try again" << std::endl;
      std::this_thread::sleep_for(std::chrono::seconds(1));
      continue;
    }
  }
  // 启动异步读取线程
  running_ = true;
  read_thread_ = std::thread(&SerialTransport::readThreadFunc, this);
  return true;
}

bool SerialTransport::configure(uint32_t baudrate) {
  struct termios tty;
  memset(&tty, 0, sizeof(tty));

  // 获取当前配置
  if (tcgetattr(fd_, &tty) != 0) {
    return false;
  }

  // 基础配置
  tty.c_cflag &= ~PARENB;  // 无奇偶校验
  tty.c_cflag &= ~CSTOPB;  // 1位停止位
  tty.c_cflag &= ~CSIZE;
  tty.c_cflag |= CS8;             // 8位数据位
  tty.c_cflag &= ~CRTSCTS;        // 无硬件流控
  tty.c_cflag |= CREAD | CLOCAL;  // 启用接收，忽略控制线

  // 输入模式配置
  tty.c_iflag &= ~(IXON | IXOFF | IXANY);  // 无软件流控
  tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

  // 输出模式配置
  tty.c_oflag &= ~OPOST;  // 原始输出模式
  tty.c_oflag &= ~ONLCR;

  // 本地模式配置
  tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHONL | ISIG);

  // 波特率设置
  speed_t speed;
  switch (baudrate) {
    case 9600:
      speed = B9600;
      break;
    case 19200:
      speed = B19200;
      break;
    case 38400:
      speed = B38400;
      break;
    case 57600:
      speed = B57600;
      break;
    case 115200:
      speed = B115200;
      break;
    default:
      return false;
  }
  cfsetispeed(&tty, speed);
  cfsetospeed(&tty, speed);

  // 应用配置
  if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
    return false;
  }
  return true;
}

void SerialTransport::start() {
  ctx_ = std::make_shared<ThreadIoMgr>();
  ctx_->t = std::thread([this]() {
    std::string thread_name = "tty_asend";
    pthread_setname_np(pthread_self(), thread_name.c_str());
    ctx_->io.run();
  });
}

int SerialTransport::send(const std::vector<uint8_t>& data) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (fd_ < 0) return -1;
  return write(fd_, data.data(), data.size());
}

void SerialTransport::async_send(const std::vector<uint8_t>& data) {
  ctx_->io.post([this, data]() { send(data); });
}

void SerialTransport::setCallback(const DataCallback& callback) {
  std::lock_guard<std::mutex> lock(mutex_);
  callback_ = callback;
}

void SerialTransport::stop() {
  if (running_) {
    running_ = false;
    if (read_thread_.joinable()) {
      read_thread_.join();
    }
  }
  ctx_->io.stop();
  if (ctx_->t.joinable()) {
    ctx_->t.join();
  }
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

void SerialTransport::readThreadFunc() {
  std::vector<uint8_t> buffer(1024);
  while (running_) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd_, &read_fds);

    // 设置超时
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;  // 100ms

    // 等待数据到达
    int ret = select(fd_ + 1, &read_fds, nullptr, nullptr, &timeout);
    if (ret < 0) {
      if (errno != EINTR) break;  // 错误退出
    } else if (ret > 0 && FD_ISSET(fd_, &read_fds)) {
      // 读取数据
      ssize_t n = read(fd_, buffer.data(), buffer.size());
      if (n > 0) {
        std::vector<uint8_t> data(buffer.begin(), buffer.begin() + n);
        ctx_->io.post([this, data] {
          if (callback_) callback_(data);  // 触发回调
        });
      } else if (n == 0) {
        // 设备断开
        break;
      }
    }
  }
}
