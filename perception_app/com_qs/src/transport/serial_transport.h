#pragma once

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include <asio.hpp>
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#if 0
#define START_FRAME_MAGIC_ID (0x00BE0001)
#define CONTENT_FRAME_MAGIC_ID (0x00000001)
#define END_FRAME_MAGIC_ID (0xEF000001)
#define ARRAY_LENGTH(arr) (sizeof(arr) / sizeof((arr)[0]))

#pragma pack(push, 4)
struct PacketHeader {
  uint32_t packet_magic_id;
  uint16_t packet_id;
  uint16_t packet_total_num;
  uint32_t packet_byte_size;
  uint32_t total_byte_size;
  uint32_t crc;
};
#pragma pack(pop)
#endif

struct ThreadIoMgr {
  asio::io_context io;
  asio::executor_work_guard<asio::io_context::executor_type> guard;
  ThreadIoMgr() : guard(asio::executor_work_guard<asio::io_context::executor_type>(io.get_executor())) {}
  std::thread t;
};

class SerialTransport {
 public:
  // 数据接收回调类型
  using DataCallback = std::function<void(const std::vector<uint8_t>&)>;

  SerialTransport(const std::string& port);

  ~SerialTransport();

  // 禁用拷贝和移动
  SerialTransport(const SerialTransport&) = delete;
  SerialTransport& operator=(const SerialTransport&) = delete;

  /**
   * @brief 发送数据
   * @param data 待发送的二进制数据
   * @return 实际发送的字节数（-1表示失败）
   */
  int send(const std::vector<uint8_t>& data);

  /**
   * @brief 注册数据接收回调
   * @param callback 回调函数
   */
  void setCallback(const DataCallback& callback);

  void start();
  /**
   * @brief 手动关闭串口
   */
  void stop();

  void async_send(const std::vector<uint8_t>& data);

  bool init(uint32_t baudrate, int retry_times);

 private:
  // 串口配置初始化
  bool configure(uint32_t baudrate);

  // 异步读取线程函数
  void readThreadFunc();

  std::string port_;                  // 设备路径
  int fd_ = -1;                       // 文件描述符
  std::atomic<bool> running_{false};  // 线程运行标志
  std::thread read_thread_;           // 异步读取线程
  DataCallback callback_;             // 数据回调
  std::mutex mutex_;                  // 线程安全锁

  std::shared_ptr<ThreadIoMgr> ctx_;
};
