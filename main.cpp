#include "SystemMonitor.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <io.h>
#include <fcntl.h>

int main() {
    // Set console to UTF-16 for wide character support
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stdin), _O_U16TEXT);
    
    std::wcout << L"Starting System Resource Monitor...\n";
    std::wcout << L"Press Ctrl+C to exit\n\n";
    
    // Small delay to allow user to see the message
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    while (true) {
        // Clear screen
        SystemMonitor::ClearScreen();
        
        // Get system information
        SystemInfo sysInfo = SystemMonitor::GetSystemInfo();
        
        // Display header with system info
        SystemMonitor::DisplayHeader(sysInfo);
        
        // Get process list
        std::vector<ProcessInfo> processes = SystemMonitor::GetProcessList(sysInfo.processorCount);
        
        // Display process list
        SystemMonitor::DisplayProcessList(processes);
        
        // Refresh every 2 seconds
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }
    
    return 0;
}

