So basically, after 3 years of not posting anything since my last "Kernel Driver" release, I've decided to drop another method, mostly for newcomers who want to start getting into Kernel to Usermode communication.

This release demonstrates a Shared Memory communication method, which is generally faster and stealthier than standard IOCTL (DeviceIoControl). To make it practical, I've included a fully functional Counter-Strike 2 Bhop example. While this technically includes a cheat feature, the main focus here is on the underlying communication architecture.

🚀 [B]How It Works (The "Sauce")[/B]
Instead of using standard IOCTLs (which involve repeated syscall overhead for every read/write), this project uses a Shared Memory Section.

The Driver creates a named section in memory (\BaseNamedObjects\NiohSharedV6). -> Highly detected on good AC ( HARDCODED )

[LIST]
[*]The Usermode Client maps this section into its own memory space.
[*]Communication happens by simply writing to this memory buffer.
[*]Client sets Status = 1 -> Driver reads memory.
[*]Client sets Status = 3 -> Driver writes memory.
[*]Driver resets Status = 2 -> Operation done.
[/LIST]

This acts like a high-speed bridge where both the cheat and the driver look at the exact same piece of RAM.

🛠️ [B]The Driver (main.c)[/B]
The kernel component is a system thread that loops and monitors the shared memory buffer for commands.

Key Features:

[LIST]
[*]Safety Checks (Anti-BSOD): Before attempting any memory operation, the driver validates the target address. It checks if the address is too small (Null Pointer protection) or too large (Kernel Space protection), preventing fatal system crashes.

[*]Dynamic PID: The driver doesn't hardcode the Game PID. It waits for the Usermode client to write the target ProcessId into the shared memory structure.

[*]MmCopyVirtualMemory: Uses standard kernel APIs to safely copy memory between the game process and our shared buffer.
[/LIST]

[B]Code Breakdown:[/B]
[CODE]
// The driver loop monitors 'SharedMemory->Status'
if (SharedMemory->Status == 1) {
    // READ COMMAND: Copy from Game -> Shared Buffer
    MmCopyVirtualMemory(GameProcess, TargetAddress, CurrentProcess, Buffer, Size, ...);
}
else if (SharedMemory->Status == 3) {
    // WRITE COMMAND: Copy from Shared Buffer -> Game
    MmCopyVirtualMemory(CurrentProcess, Buffer, GameProcess, TargetAddress, Size, ...);
}[/CODE]

💻[B] The Client (client.cpp)[/B]
The usermode application acts as the "controller". It finds the game, calculates offsets, and tells the driver what to do.

Key Features:

[LIST]
[*]Handshake Protocol: It performs a handshake (Status 99 -> 100) to ensure the driver is loaded and listening before sending any commands.

[*]Bhop Logic: A simple infinite loop that checks the Spacebar state and writes to the dwForceJump offset in the game memory.

[*]Robustness: Uses a template-based WriteMemory helper that handles the synchronization with the driver (waiting for the driver to acknowledge the command).
[/LIST]

[B]Code Breakdown:[/B]
[CODE]
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
[/CODE]

I have provided the source code and the general steps to get started. You are expected to have the prerequisite knowledge to compile and load the driver yourself. Thanks <3
