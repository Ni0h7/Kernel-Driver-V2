
# 🌉 Shared Memory Kernel Bridge + CS2 Bhop Example

> **A high-performance kernel-to-usermode communication proof of concept.**

After 3 years since my last "Kernel Driver" release, I've decided to drop another method, designed primarily for newcomers who want to understand **Kernel Communication** beyond the standard methods.

This project demonstrates a **Shared Memory** communication architecture. It is generally faster and stealthier than standard IOCTL (`DeviceIoControl`) methods because it eliminates repeated syscall overhead. To demonstrate its practicality, a fully functional **Counter-Strike 2 Bhop** is included.

---

## 🚀 How It Works (The "Sauce")

Instead of using standard IOCTLs (which involve context switches and overhead for every single read/write), this project uses a **Shared Memory Section**.

1. **The Driver** creates a named section in physical memory (`\BaseNamedObjects\NiohSharedV6`).
2. **The Usermode Client** maps this section into its own virtual memory space.
3. **Communication** happens by simply reading/writing variables in this shared buffer.

This acts like a high-speed bridge where both the cheat and the driver look at the exact same piece of RAM.

### Communication Flow

1. **Client** writes request data (PID, Address, Size) to the buffer.
2. **Client** sets `Status = 1` (Read) or `Status = 3` (Write).
3. **Driver** (monitoring the buffer) detects the change.
4. **Driver** executes the memory operation via `MmCopyVirtualMemory`.
5. **Driver** resets `Status = 2` (Done).

---

## 🛠️ The Driver (`Kernel.sys`)

The kernel component is a system thread that loops and monitors the shared memory buffer for commands.

### Key Features

| Feature | Description |
| --- | --- |
| **🛡️ Anti-BSOD** | Validates addresses before access. Checks for Null Pointers (Too small) and Kernel Space (Too large) to prevent system crashes. |
| **🔄 Dynamic PID** | Does not hardcode the game PID. It waits for the Usermode client to provide the target `ProcessId`. |
| **⚡ MmCopyVirtualMemory** | Uses standard, safe kernel APIs to copy memory between the game process and the shared buffer. |

### Code Breakdown

```c
// The driver loop monitors 'SharedMemory->Status'
if (SharedMemory->Status == 1) {
    // READ COMMAND: Copy from Game -> Shared Buffer
    MmCopyVirtualMemory(GameProcess, TargetAddress, CurrentProcess, Buffer, Size, ...);
}
else if (SharedMemory->Status == 3) {
    // WRITE COMMAND: Copy from Shared Buffer -> Game
    MmCopyVirtualMemory(CurrentProcess, Buffer, GameProcess, TargetAddress, Size, ...);
}

```

---

## 💻 The Client (`Client.exe`)

The usermode application acts as the "controller". It finds the game process, calculates offsets, and dispatches commands to the driver.

### Key Features

* **🤝 Handshake Protocol:** Performs a startup check (`Status 99` -> `100`) to ensure the driver is loaded and ready.
* **🐰 Bhop Logic:** A simple loop that checks `Spacebar` state and writes to the `dwForceJump` offset.
* **🧱 Robust Synchronization:** Uses a template-based helper that uses spinlocks (`_mm_pause()`) to wait for driver acknowledgement.

### Code Breakdown

```cpp
// 1. Connect to the Shared Memory Section created by the driver
HANDLE hMapFile = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, "Global\\NiohSharedV6");
pMem = (SHARED_MEMORY*)MapViewOfFile(hMapFile, ...);

// 2. Send a Write Command (Bhop)
// We set the Status to 3, letting the driver know we want to write data.
pMem->Address = clientBase + dwForceJump;
pMem->Status = 3; 

// 3. Wait for Driver
while (pMem->Status == 3) { 
    _mm_pause(); // Spinlock until driver processes the request
}

```

---

## ⚠️ Security & Detection Warning

> [!WARNING]
> **READ THIS BEFORE USING ON MAIN ACCOUNTS**
> The driver currently creates a **Named Section** (`\BaseNamedObjects\NiohSharedV6`).
> * **Detection Risk:** High. Good Anti-Cheats (EAC, BattlEye, Vanguard) scan the Object Directory. A hardcoded name like this is a "signature" and allows them to easily flag the driver.
> * **Recommendation:** For use in a secured environment, you must **randomize the name** or implement **unnamed (anonymous) memory mapping**.
> 
> 

---

## 📦 How to Use

I have provided the source code and the general steps. You are expected to have the prerequisite knowledge to compile and load the driver yourself.

1. **Compile the Driver:** Build `topg.sys` (Release x64).
2. **Load the Driver:**
* *Method A:* Use a manual mapper (e.g., `kdmapper`).
* *Method B:* Enable Test Signing and use `sc start` (Not recommended for AC).


3. **Update Offsets:** Check `client.cpp` and update `dwForceJump` for the latest CS2 patch.
4. **Run Client:** Launch CS2, then run `Client.exe` as Administrator.

---

## 📜 Disclaimer & Credits

**Disclaimer:** This software is for **educational purposes only**. The author is not responsible for any bans or damages caused by the use of this software.

**Credits:**

* **Me** - Logic - Implementation
* **UnknownCheats** - For the endless resources
* https://www.unknowncheats.me/forum/c-and-c-/733619-kernel-driver-v2.html

<p align="center">Made with ❤️ for the community</p>
