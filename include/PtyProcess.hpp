#pragma once
#include <string>
#include <unistd.h>
#include <termios.h>

class PtyProcess {
public:
    PtyProcess();
    ~PtyProcess();

    bool start(const std::string& shell = "/bin/bash", const std::string& cwd = "", int cols = 80, int rows = 24);
    void resize(int cols, int rows);
    ssize_t writeData(const char* data, size_t length);
    void stop();

    int getMasterFd() const { return masterFd_; }
    pid_t getPid() const { return childPid_; }
    bool isRunning() const { return running_; }

private:
    int masterFd_ = -1;
    pid_t childPid_ = -1;
    bool running_ = false;
};
