#include <iostream>
#include <chrono>
#include <thread>
static int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
int main() {
    int64_t a = now_ms();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    int64_t b = now_ms();
    std::cout << "a=" << a << " b=" << b << " diff=" << (b-a) << "\n";
    return 0;
}
