#include "ProcessRunner.h"
#include <string>
#include <thread>
#include <vector>

// Isolate Windows.h here
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace ProcessRunner 
{
    void RunBuildCommand(const std::string& cmd, std::function<void(const std::string&, bool)> onOutput, std::function<void(bool)> onComplete)
    {
        std::thread([cmd, onOutput, onComplete]() 
        {
            SECURITY_ATTRIBUTES sa;
            sa.nLength = sizeof(SECURITY_ATTRIBUTES);
            sa.bInheritHandle = TRUE;
            sa.lpSecurityDescriptor = NULL;

            HANDLE hRead, hWrite;
            if (!CreatePipe(&hRead, &hWrite, &sa, 0))
            {
                if (onOutput) onOutput("Failed to create pipe.", true);
                if (onComplete) onComplete(false);
                return;
            }

            STARTUPINFOA si;
            ZeroMemory(&si, sizeof(STARTUPINFO));
            si.cb = sizeof(STARTUPINFO);
            si.hStdError = hWrite;
            si.hStdOutput = hWrite;
            si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;

            PROCESS_INFORMATION pi;
            ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));

            std::string cmdMutable = "cmd.exe /C " + cmd;

            if (!CreateProcessA(NULL, cmdMutable.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
            {
                if (onOutput) onOutput("Failed to create process.", true);
                CloseHandle(hRead);
                CloseHandle(hWrite);
                if (onComplete) onComplete(false);
                return;
            }

            CloseHandle(hWrite);

            DWORD bytesRead;
            char buffer[128];
            std::string currentLine;
            
            while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0)
            {
                buffer[bytesRead] = '\0';
                currentLine += buffer;

                size_t pos;
                while ((pos = currentLine.find('\n')) != std::string::npos)
                {
                    std::string line = currentLine.substr(0, pos);
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    
                    if (onOutput) onOutput(line, false);
                    currentLine.erase(0, pos + 1);
                }
            }

            if (!currentLine.empty())
            {
                if (onOutput) onOutput(currentLine, false);
            }

            WaitForSingleObject(pi.hProcess, INFINITE);
            
            DWORD exitCode = 0;
            GetExitCodeProcess(pi.hProcess, &exitCode);

            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            CloseHandle(hRead);

            if (onComplete) onComplete(exitCode == 0);

        }).detach();
    }
}
