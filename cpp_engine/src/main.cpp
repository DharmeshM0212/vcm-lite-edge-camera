#include <chrono>
#include <iostream>
#include <thread>

int main() {
    unsigned long long frame_id = 0;

    while (true) {
        std::cout << "{\"frame_id\":" << frame_id << ",\"status\":\"engine_alive\"}" << std::endl;
        frame_id++;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}