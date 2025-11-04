#include "SystemMonitor.h"
#include <algorithm>
#include <map>

// Static member initialization
ULONGLONG SystemMonitor::lastTotalTime = 0;
ULONGLONG SystemMonitor::lastIdleTime = 0;
bool SystemMonitor::cpuInitialized = false;
std::map<DWORD, ULONGLONG> SystemMonitor::processLastTimes;
ULONGLONG SystemMonitor::lastSystemTime = 0;

SystemInfo SystemMonitor::GetSystemInfo() {
    SystemInfo info = {0};
    
    // Get processor count
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    info.processorCount = sysInfo.dwNumberOfProcessors;
    
    // Get memory information
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memStatus)) {
        info.totalMemory = memStatus.ullTotalPhys / 1024;  // Convert to KB
        info.availableMemory = memStatus.ullAvailPhys / 1024;
        info.usedMemory = info.totalMemory - info.availableMemory;
        info.memoryUsagePercent = memStatus.dwMemoryLoad;
    }
    
    // Calculate CPU usage
    info.cpuUsagePercent = CalculateCpuUsage();
    
    return info;
}

ULONGLONG SystemMonitor::GetTotalSystemMemory() {
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memStatus)) {
        return memStatus.ullTotalPhys / 1024;  // KB
    }
    return 0;
}

ULONGLONG SystemMonitor::GetUsedSystemMemory() {
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memStatus)) {
        ULONGLONG total = memStatus.ullTotalPhys / 1024;
        ULONGLONG available = memStatus.ullAvailPhys / 1024;
        return total - available;
    }
    return 0;
}

ULONGLONG SystemMonitor::FileTimeToULONGLONG(const FILETIME& ft) {
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return uli.QuadPart;
}

double SystemMonitor::CalculateCpuUsage() {
    FILETIME idleTime, kernelTime, userTime;
    
    if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        return 0.0;
    }
    
    ULONGLONG idle = FileTimeToULONGLONG(idleTime);
    ULONGLONG kernel = FileTimeToULONGLONG(kernelTime);
    ULONGLONG user = FileTimeToULONGLONG(userTime);
    ULONGLONG total = kernel + user;
    
    if (!cpuInitialized) {
        lastTotalTime = total;
        lastIdleTime = idle;
        cpuInitialized = true;
        return 0.0;
    }
    
    ULONGLONG totalDiff = total - lastTotalTime;
    ULONGLONG idleDiff = idle - lastIdleTime;
    
    if (totalDiff == 0) {
        return 0.0;
    }
    
    lastTotalTime = total;
    lastIdleTime = idle;
    
    double cpuPercent = 100.0 * (1.0 - (double)idleDiff / (double)totalDiff);
    return cpuPercent;
}

std::vector<ProcessInfo> SystemMonitor::GetProcessList(DWORD processorCount) {
    std::vector<ProcessInfo> processes;
    
    // Get system time once for all processes
    FILETIME systemIdleTime, systemKernelTime, systemUserTime;
    ULONGLONG currentSystemTime = 0;
    if (GetSystemTimes(&systemIdleTime, &systemKernelTime, &systemUserTime)) {
        ULONGLONG systemKernel = FileTimeToULONGLONG(systemKernelTime);
        ULONGLONG systemUser = FileTimeToULONGLONG(systemUserTime);
        currentSystemTime = systemKernel + systemUser;
    }
    
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS | TH32CS_SNAPMODULE, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return processes;
    }
    
    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);
    
    ULONGLONG totalMemory = GetTotalSystemMemory();
    
    if (Process32FirstW(hSnapshot, &pe32)) {
        do {
            ProcessInfo procInfo;
            procInfo.pid = pe32.th32ProcessID;
            procInfo.name = pe32.szExeFile;
            
            // Get memory usage for this process
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe32.th32ProcessID);
            if (hProcess != NULL) {
                PROCESS_MEMORY_COUNTERS_EX pmc;
                if (GetProcessMemoryInfo(hProcess, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
                    procInfo.memoryUsage = pmc.WorkingSetSize / 1024;  // Convert to KB
                    if (totalMemory > 0) {
                        procInfo.memoryPercent = (procInfo.memoryUsage * 100) / totalMemory;
                    } else {
                        procInfo.memoryPercent = 0;
                    }
                } else {
                    procInfo.memoryUsage = 0;
                    procInfo.memoryPercent = 0;
                }
                
                // Calculate CPU usage for this process
                FILETIME creationTime, exitTime, kernelTime, userTime;
                
                if (GetProcessTimes(hProcess, &creationTime, &exitTime, &kernelTime, &userTime)) {
                    ULONGLONG processKernel = FileTimeToULONGLONG(kernelTime);
                    ULONGLONG processUser = FileTimeToULONGLONG(userTime);
                    ULONGLONG processTotal = processKernel + processUser;
                    
                    // Check if we have previous data for this process
                    auto it = processLastTimes.find(procInfo.pid);
                    if (it != processLastTimes.end() && lastSystemTime > 0 && currentSystemTime > lastSystemTime) {
                        ULONGLONG processDiff = processTotal - it->second;
                        ULONGLONG systemDiff = currentSystemTime - lastSystemTime;
                        
                        if (systemDiff > 0) {
                            // Calculate CPU percentage: (process time / system time) * 100 * processor count
                            procInfo.cpuPercent = (100.0 * processDiff * processorCount) / systemDiff;
                        } else {
                            procInfo.cpuPercent = 0.0;
                        }
                    } else {
                        procInfo.cpuPercent = 0.0;
                    }
                    
                    // Update tracking
                    processLastTimes[procInfo.pid] = processTotal;
                } else {
                    procInfo.cpuPercent = 0.0;
                }
                
                CloseHandle(hProcess);
            } else {
                procInfo.memoryUsage = 0;
                procInfo.memoryPercent = 0;
                procInfo.cpuPercent = 0.0;
            }
            
            procInfo.status = L"Running";
            
            processes.push_back(procInfo);
        } while (Process32NextW(hSnapshot, &pe32));
    }
    
    CloseHandle(hSnapshot);
    
    // Update system time after processing all processes
    if (currentSystemTime > 0) {
        lastSystemTime = currentSystemTime;
    }
    
    // Sort by memory usage (descending)
    std::sort(processes.begin(), processes.end(), 
        [](const ProcessInfo& a, const ProcessInfo& b) {
            return a.memoryUsage > b.memoryUsage;
        });
    
    return processes;
}

void SystemMonitor::ClearScreen() {
    system("cls");
}

std::wstring SystemMonitor::FormatBytes(SIZE_T bytes) {
    std::wstringstream ss;
    if (bytes < 1024) {
        ss << bytes << L" KB";
    } else if (bytes < 1024 * 1024) {
        ss << std::fixed << std::setprecision(2) << (bytes / 1024.0) << L" MB";
    } else {
        ss << std::fixed << std::setprecision(2) << (bytes / (1024.0 * 1024.0)) << L" GB";
    }
    return ss.str();
}

std::wstring SystemMonitor::FormatPercent(double percent) {
    std::wstringstream ss;
    ss << std::fixed << std::setprecision(1) << percent << L"%";
    return ss.str();
}

void SystemMonitor::DisplayHeader(const SystemInfo& sysInfo) {
    std::wcout << L"═══════════════════════════════════════════════════════════════════════════════\n";
    std::wcout << L"                    SYSTEM RESOURCE MONITOR\n";
    std::wcout << L"═══════════════════════════════════════════════════════════════════════════════\n";
    std::wcout << L"CPU Usage: " << FormatPercent(sysInfo.cpuUsagePercent) << L"  |  ";
    std::wcout << L"Processors: " << sysInfo.processorCount << L"  |  ";
    std::wcout << L"Memory Usage: " << FormatPercent(sysInfo.memoryUsagePercent) << L"\n";
    std::wcout << L"Total Memory: " << FormatBytes(sysInfo.totalMemory) << L"  |  ";
    std::wcout << L"Used: " << FormatBytes(sysInfo.usedMemory) << L"  |  ";
    std::wcout << L"Available: " << FormatBytes(sysInfo.availableMemory) << L"\n";
    std::wcout << L"═══════════════════════════════════════════════════════════════════════════════\n";
    std::wcout << L"\n";
}

void SystemMonitor::DisplayProcessList(const std::vector<ProcessInfo>& processes) {
    // Table header
    std::wcout << L"PID        CPU%      Memory%   Memory Usage   Process Name\n";
    std::wcout << L"────────────────────────────────────────────────────────────────────────────\n";
    
    // Display top 20 processes
    size_t count = (processes.size() < 20) ? processes.size() : 20;
    for (size_t i = 0; i < count; ++i) {
        const ProcessInfo& proc = processes[i];
        std::wcout << std::setw(10) << std::left << proc.pid;
        std::wcout << std::setw(10) << std::left << FormatPercent(proc.cpuPercent);
        std::wcout << std::setw(10) << std::left << proc.memoryPercent;
        std::wcout << std::setw(15) << std::left << FormatBytes(proc.memoryUsage);
        std::wcout << proc.name << L"\n";
    }
    
    std::wcout << L"\n";
    std::wcout << L"Total Processes: " << processes.size() << L"\n";
    std::wcout << L"Press Ctrl+C to exit\n";
}

