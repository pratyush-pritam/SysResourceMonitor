cmake_minimum_required(VERSION 3.10)
project(SystemResourceMonitor)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add executable
add_executable(SystemMonitor
    main.cpp
    SystemMonitor.cpp
)

# Link Windows libraries
target_link_libraries(SystemMonitor
    psapi
)

# Windows-specific settings
if(WIN32)
    target_compile_definitions(SystemMonitor PRIVATE _WIN32_WINNT=0x0601)
endif()

