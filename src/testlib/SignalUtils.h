#pragma once

namespace testlib {
void waitForSIGINT();
void waitForKeyPress(const char* prompt="Press any key to continue...");
}  // namespace testlib
