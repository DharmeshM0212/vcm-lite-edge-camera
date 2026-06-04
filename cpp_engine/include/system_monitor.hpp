#pragma once

class ProcessMonitor {
public:
    ProcessMonitor();

    double cpu_percent();
    double ram_mb() const;

private:
    long clock_ticks_;
    long page_size_;
    unsigned long long last_process_ticks_;
    unsigned long long last_total_ticks_;
    double last_cpu_percent_;

    unsigned long long read_process_ticks() const;
    unsigned long long read_total_cpu_ticks() const;
};