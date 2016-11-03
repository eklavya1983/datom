#include <iostream>
#include <csignal>
#include <thread>
#include <testlib/SignalUtils.h>

namespace testlib {

static bool SIGINT_SET = false;
static void signalHandler(int signum)
{
    SIGINT_SET = true;
}
void waitForSIGINT() {
    signal(SIGINT, signalHandler);
    std::cout << "Waiting for SIGINT....\n";
    while (!SIGINT_SET) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void waitForKeyPress(const char *prompt)
{
    std::cout << prompt << "\n";
    std::cin.get();
}

}  // namespace testlib
