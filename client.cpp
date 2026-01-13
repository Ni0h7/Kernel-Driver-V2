#include <iostream>
#include <Windows.h>
#include <TlHelp32.h>
#include <vector>

// --- SHARED MEMORY (DRIVER INTERFACE) ---
struct SHARED_MEMORY {
    volatile ULONG Status;
    ULONG ProcessId;
    ULONGLONG Address;
    ULONG Size;
    UCHAR Buffer[128];
};

// =============================================================
//  OFFSET CONFIGURATION
// =============================================================

constexpr std::ptrdiff_t dwForceJump = 0x1BE88B0;

// =============================================================

// --- HELPER: WRITE (Kernel Write via Status 3) ---
template <typename T>
void WriteMemory(SHARED_MEMORY* pMem, uintptr_t address, T value) {
    if (address == 0) return;

    // Copy data to buffer
    memcpy(pMem->Buffer, &value, sizeof(T));

    // Set address and size
    pMem->Address = address;
    pMem->Size = sizeof(T);

    // Send Status 3 (Driver Write Command)
    pMem->Status = 3;

    // Wait until driver is finished (Status != 3)
    int timeout = 0;
    while (pMem->Status == 3) {
        if (++timeout > 10000) return;
        _mm_pause();
    }
}

uintptr_t GetModuleBaseAddress(DWORD procId, const wchar_t* modName) {
    uintptr_t modBaseAddr = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, procId);
    if (hSnap != INVALID_HANDLE_VALUE) {
        MODULEENTRY32W modEntry;
        modEntry.dwSize = sizeof(modEntry);
        if (Module32FirstW(hSnap, &modEntry)) {
            do {
                if (!_wcsicmp(modEntry.szModule, modName)) {
                    modBaseAddr = (uintptr_t)modEntry.modBaseAddr;
                    break;
                }
            } while (Module32NextW(hSnap, &modEntry));
        }
        CloseHandle(hSnap);
    }
    return modBaseAddr;
}

int main() {
    SetConsoleTitleA("Nioh Spam-Bhop (Driver Mode)");

    // 1. Connect Memory Host
    HANDLE hMapFile = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(SHARED_MEMORY), "Global\\NiohSharedV6");
    if (!hMapFile && GetLastError() == ERROR_ALREADY_EXISTS) hMapFile = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, "Global\\NiohSharedV6");

    if (!hMapFile) {
        std::cout << "[FEHLER] Konnte Shared Memory nicht oeffnen!" << std::endl;
        system("pause");
        return 0;
    }

    SHARED_MEMORY* pMem = (SHARED_MEMORY*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SHARED_MEMORY));

    if (!pMem) {
        std::cout << "[FEHLER] MapViewOfFile fehlgeschlagen!" << std::endl;
        system("pause");
        return 0;
    }

    pMem->Status = 0;

    std::cout << "[*] Waiting for Driver (Handshake)..." << std::endl;
    // We set Status to 99 and wait for 100
    while (true) {
        pMem->Status = 99;
        Sleep(100);
        if (pMem->Status == 100) break;
    }
    std::cout << "[+] Driver connected!" << std::endl;

    // 2. Search for CS2 process
    DWORD pid = 0;
    std::cout << "[*] Searching Counter-Strike 2..." << std::endl;
    while (pid == 0) {
        HWND hwnd = FindWindowA(NULL, "Counter-Strike 2");
        if (hwnd) {
            GetWindowThreadProcessId(hwnd, &pid);
            // Pass PID to driver
            pMem->ProcessId = pid;
        }
        Sleep(100);
    }

    // 3. Find Client.dll base address
    uintptr_t clientBase = GetModuleBaseAddress(pid, L"client.dll");
    while (clientBase == 0) {
        Sleep(100);
        clientBase = GetModuleBaseAddress(pid, L"client.dll");
    }

    std::cout << "------------------------------------------------" << std::endl;
    std::cout << "SPAM BHOP AKTIV " << std::endl;
    std::cout << "Jump Offset: 0x" << std::hex << dwForceJump << std::dec << std::endl;
    std::cout << "Hold Spacebar" << std::endl;
    std::cout << "------------------------------------------------" << std::endl;

    const int FORCE_JUMP_ACTIVE = 65537;
    const int FORCE_JUMP_INACTIVE = 256;
    const int JUMP_DELAY = 15; 

    // 4. MAIN LOOP
    while (true) {
        // We ONLY check the key. No flags, no LocalPlayer.
        // GetAsyncKeyState checks globally if the key is pressed.
        bool spacePressed = (GetAsyncKeyState(VK_SPACE) & 0x8000);

        if (spacePressed) {
            // 1. Send Jump (+jump)
            WriteMemory<int>(pMem, clientBase + dwForceJump, FORCE_JUMP_ACTIVE);

            // 2. Wait (Simulates timeSinceLastAction from C#)
            Sleep(JUMP_DELAY);

            // 3. Send Reset (-jump)
            WriteMemory<int>(pMem, clientBase + dwForceJump, FORCE_JUMP_INACTIVE);

            // 4. Wait
            Sleep(JUMP_DELAY);
        }
        else {
            // Save CPU when doing nothing
            Sleep(1);
        }
    }
    return 0;
}