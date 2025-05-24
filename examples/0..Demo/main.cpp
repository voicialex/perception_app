#include "ImageReceiver.hpp"
#include <chrono>
#include <thread>

int main([[maybe_unused]] int argc, [[maybe_unused]] char **argv) {
    ImageReceiver receiver;
    receiver.run();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    return 0;
}
