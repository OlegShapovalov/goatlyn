#include "PtyProcess.hpp"
#include <iostream>
#include <cstring>
#include <pty.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <fcntl.h>
#include <poll.h>
#include <cerrno>
#include <sys/wait.h>
#include <cerrno>

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
}

ssize_t PtyProcess::writeData(const char* data, size_t length) {
    if (masterFd_ < 0 || !data || length == 0) return -1;

    size_t totalWritten = 0;
    while (totalWritten < length) {
        ssize_t bytes = write(masterFd_, data + totalWritten, length - totalWritten);
        if (bytes > 0) {
            totalWritten += bytes;
        } else if (bytes < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Буфер ядра полон: ждем готовности сокета к записи
                struct pollfd pfd{};
                pfd.fd = masterFd_;
                pfd.events = POLLOUT;
                int ret = poll(&pfd, 1, 100);
                if (ret <= 0) {
                    break;
                }
                continue;
            }
            return -1;
        } else {
            break;
        }
    }
    return totalWritten;
}

void PtyProcess::stop() {
    if (!running_ && childPid_ <= 0) return;
    running_ = false;

    // 1. Закрываем master fd — ядро отправит SIGHUP сессии на pty.
    if (masterFd_ >= 0) {
        close(masterFd_);
        masterFd_ = -1;
    }

    // 2. Явный SIGHUP — на случай, если дочерний процесс в другой
    //    process group и pty-сигнал до него не дошёл.
    if (childPid_ > 0) {
        kill(childPid_, SIGHUP);
    }

    // 3. Ждём завершения ребёнка, чтобы не осталось зомби.
    if (childPid_ > 0) {
        const int maxAttempts = 50;   // ~500 мс максимум
        for (int i = 0; i < maxAttempts; ++i) {
            int status = 0;
            pid_t r = waitpid(childPid_, &status, WNOHANG);
            if (r == childPid_) {
                childPid_ = -1;
                return;
            }
            if (r == -1) {
                // ECHILD — ребёнок уже собран кем-то другим (Qt)
                childPid_ = -1;
                return;
            }
            // r == 0 — ещё жив, ждём
            usleep(10 * 1000);
        }

        // Таймаут: SIGHUP не сработал — убиваем жёстко.
        kill(childPid_, SIGKILL);
        waitpid(childPid_, nullptr, 0);
        childPid_ = -1;
    }
}