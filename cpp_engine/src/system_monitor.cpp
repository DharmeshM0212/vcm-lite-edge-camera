#include "system_monitor.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <unistd.h>
#endif

ProcessMonitor::ProcessMonitor()
    : clock_ticks_(100),
      page_size_(4096),
      last_process_ticks_(0),
      last_total_ticks_(0),
      last_cpu_percent_(0.0) {
#ifndef _WIN32
    long ticks = sysconf(_SC_CLK_TCK);
    long pages = sysconf(_SC_PAGESIZE);

    if (ticks > 0) {
        clock_ticks_ = ticks;
    }

    if (pages > 0) {
        page_size_ = pages;
    }
#endif

    last_process_ticks_ = read_process_ticks();
    last_total_ticks_ = read_total_cpu_ticks();
}

unsigned long long ProcessMonitor::read_process_ticks() const {
#ifdef _WIN32
    FILETIME create_time;
    FILETIME exit_time;
    FILETIME kernel_time;
    FILETIME user_time;

    if (!GetProcessTimes(GetCurrentProcess(), &create_time, &exit_time, &kernel_time, &user_time)) {
        return 0;
    }

    ULARGE_INTEGER kernel;
    ULARGE_INTEGER user;

    kernel.LowPart = kernel_time.dwLowDateTime;
    kernel.HighPart = kernel_time.dwHighDateTime;
    user.LowPart = user_time.dwLowDateTime;
    user.HighPart = user_time.dwHighDateTime;

    return static_cast<unsigned long long>((kernel.QuadPart + user.QuadPart) / 100000);
#else
    std::ifstream file("/proc/self/stat");

    if (!file.is_open()) {
        return 0;
    }

    std::string line;
    std::getline(file, line);

    std::size_t close_paren = line.rfind(')');

    if (close_paren == std::string::npos || close_paren + 2 >= line.size()) {
        return 0;
    }

    std::string rest = line.substr(close_paren + 2);
    std::istringstream stream(rest);
    std::vector<std::string> fields;
    std::string field;

    while (stream >> field) {
        fields.push_back(field);
    }

    if (fields.size() < 15) {
        return 0;
    }

    unsigned long long utime = 0;
    unsigned long long stime = 0;

    try {
        utime = std::stoull(fields[11]);
        stime = std::stoull(fields[12]);
    } catch (...) {
        return 0;
    }

    return utime + stime;
#endif
}

unsigned long long ProcessMonitor::read_total_cpu_ticks() const {
#ifdef _WIN32
    FILETIME idle_time;
    FILETIME kernel_time;
    FILETIME user_time;

    if (!GetSystemTimes(&idle_time, &kernel_time, &user_time)) {
        return 0;
    }

    ULARGE_INTEGER idle;
    ULARGE_INTEGER kernel;
    ULARGE_INTEGER user;

    idle.LowPart = idle_time.dwLowDateTime;
    idle.HighPart = idle_time.dwHighDateTime;
    kernel.LowPart = kernel_time.dwLowDateTime;
    kernel.HighPart = kernel_time.dwHighDateTime;
    user.LowPart = user_time.dwLowDateTime;
    user.HighPart = user_time.dwHighDateTime;

    return static_cast<unsigned long long>((kernel.QuadPart + user.QuadPart) / 100000);
#else
    std::ifstream file("/proc/stat");

    if (!file.is_open()) {
        return 0;
    }

    std::string cpu;
    unsigned long long user = 0;
    unsigned long long nice = 0;
    unsigned long long system = 0;
    unsigned long long idle = 0;
    unsigned long long iowait = 0;
    unsigned long long irq = 0;
    unsigned long long softirq = 0;
    unsigned long long steal = 0;

    file >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;

    return user + nice + system + idle + iowait + irq + softirq + steal;
#endif
}

double ProcessMonitor::cpu_percent() {
    unsigned long long current_process_ticks = read_process_ticks();
    unsigned long long current_total_ticks = read_total_cpu_ticks();

    if (last_process_ticks_ == 0 || last_total_ticks_ == 0 || current_total_ticks <= last_total_ticks_) {
        last_process_ticks_ = current_process_ticks;
        last_total_ticks_ = current_total_ticks;
        return last_cpu_percent_;
    }

    unsigned long long process_delta = current_process_ticks - last_process_ticks_;
    unsigned long long total_delta = current_total_ticks - last_total_ticks_;

    last_process_ticks_ = current_process_ticks;
    last_total_ticks_ = current_total_ticks;

    if (total_delta == 0) {
        return last_cpu_percent_;
    }

    double cpu_count = 1.0;

#ifndef _WIN32
    long cores = sysconf(_SC_NPROCESSORS_ONLN);

    if (cores > 0) {
        cpu_count = static_cast<double>(cores);
    }
#else
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    cpu_count = static_cast<double>(std::max<DWORD>(1, info.dwNumberOfProcessors));
#endif

    last_cpu_percent_ = 100.0 * static_cast<double>(process_delta) / static_cast<double>(total_delta) * cpu_count;
    last_cpu_percent_ = std::clamp(last_cpu_percent_, 0.0, 100.0 * cpu_count);

    return last_cpu_percent_;
}

double ProcessMonitor::ram_mb() const {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters;

    if (!GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) {
        return 0.0;
    }

    return static_cast<double>(counters.WorkingSetSize) / (1024.0 * 1024.0);
#else
    std::ifstream file("/proc/self/statm");

    if (!file.is_open()) {
        return 0.0;
    }

    unsigned long size_pages = 0;
    unsigned long resident_pages = 0;

    file >> size_pages >> resident_pages;

    return static_cast<double>(resident_pages) * static_cast<double>(page_size_) / (1024.0 * 1024.0);
#endif
}