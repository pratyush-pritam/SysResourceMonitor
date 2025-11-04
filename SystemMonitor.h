#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <io.h>
#include <fcntl.h>

// Structure to hold process information
struct ProcessInfo {
    DWORD pid;
    std::wstring name;
    double cpuPercent;
    SIZE_T memoryUsage;  // In KB
    SIZE_T memoryPercent;
    std::wstring status;
};

// Structure to hold system information
struct SystemInfo {
    DWORD processorCount;
    double cpuUsagePercent;
    ULONGLONG totalMemory;      // In KB
    ULONGLONG usedMemory;       // In KB
    ULONGLONG availableMemory;  // In KB
    double memoryUsagePercent;
};

// System information gathering functions
class SystemMonitor {
public:
    static SystemInfo GetSystemInfo();
    static std::vector<ProcessInfo> GetProcessList(DWORD processorCount);
    static double CalculateCpuUsage();
    static void DisplayHeader(const SystemInfo& sysInfo);
    static void DisplayProcessList(const std::vector<ProcessInfo>& processes);
    static void ClearScreen();
    static std::wstring FormatBytes(SIZE_T bytes);
    static std::wstring FormatPercent(double percent);
    
private:
    static ULONGLONG GetTotalSystemMemory();
    static ULONGLONG GetUsedSystemMemory();
    static ULONGLONG FileTimeToULONGLONG(const FILETIME& ft);
    
    // For CPU calculation
    static ULONGLONG lastTotalTime;
    static ULONGLONG lastIdleTime;
    static bool cpuInitialized;
    
    // For per-process CPU calculation
    static std::map<DWORD, ULONGLONG> processLastTimes;
    static ULONGLONG lastSystemTime;
};

#endif // SYSTEM_MONITOR_H

