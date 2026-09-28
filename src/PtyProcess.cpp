#include "PtyProcess.hpp"
#include <iostream>
#include <cstring>
#include <pty.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <fcntl.h>

PtyProcess::PtyProcess() = default;

PtyProcess::~PtyProcess() {
    stop();
}

bool PtyProcess::start(const std::string& shell, const std::string& cwd, int cols, int rows) {
    struct winsize ws{};
    ws.ws_col = cols;
    ws.ws_row = rows;

    childPid_ = forkpty(&masterFd_, nullptr, nullptr, &ws);
    if (childPid_ < 0) {
        std::cerr << "forkpty failed: " << strerror(errno) << "\n";
        return false;
    }

    if (childPid_ == 0) {
        if (!cwd.empty()) {
            chdir(cwd.c_str());
        }
        setenv("TERM", "xterm-256color", 1);
        setenv("COLORTERM", "truecolor", 1);
        execl(shell.c_str(), shell.c_str(), nullptr);
        _exit(1);
    }

    int flags = fcntl(masterFd_, F_GETFL, 0);
    fcntl(masterFd_, F_SETFL, flags | O_NONBLOCK);

    running_ = true;
    return true;
}

void PtyProcess::resize(int cols, int rows) {
    if (masterFd_ < 0) return;
    struct winsize ws{};
    ws.ws_col = cols;
    ws.ws_row = rows;
    ioctl(masterFd_, TIOCSWINSZ, &ws);
    if (childPid_ > 0) {
        killpg(getpgid(childPid_), SIGWINCH);
    }
}

ssize_t PtyProcess::writeData(const char* data, size_t length) {
    if (masterFd_ < 0) return -1;
    return write(masterFd_, data, length);
}

void PtyProcess::stop() {
    if (!running_) return;
    running_ = false;
    if (masterFd_ >= 0) {
        close(masterFd_);
        masterFd_ = -1;
    }
    if (childPid_ > 0) {
        kill(childPid_, SIGHUP);
        childPid_ = -1;
    }
}
