#include <ntifs.h>
#include <ntddk.h>

// --- HELPER ---
NTSTATUS NTAPI MmCopyVirtualMemory(
    PEPROCESS SourceProcess, PVOID SourceAddress,
    PEPROCESS TargetProcess, PVOID TargetAddress,
    SIZE_T BufferSize, KPROCESSOR_MODE PreviousMode, PSIZE_T ReturnSize
);

NTSTATUS ZwOpenSection(
    PHANDLE SectionHandle, ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes
);

NTSTATUS ZwMapViewOfSection(
    HANDLE SectionHandle, HANDLE ProcessHandle, PVOID* BaseAddress,
    ULONG_PTR ZeroBits, SIZE_T CommitSize, PLARGE_INTEGER SectionOffset,
    PSIZE_T ViewSize, SECTION_INHERIT InheritDisposition, ULONG AllocationType, ULONG Win32Protect
);

#ifndef ZwCurrentProcess
#define ZwCurrentProcess() ((HANDLE)(LONG_PTR)-1)
#endif

// --- SHARED MEMORY ---
typedef struct _SHARED_MEMORY {
    volatile ULONG Status;
    ULONG ProcessId;
    ULONGLONG Address;
    ULONG Size;
    UCHAR Buffer[128];
} SHARED_MEMORY, * PSHARED_MEMORY;

HANDLE g_ThreadHandle = NULL;
BOOLEAN g_Running = TRUE;

// --- THREAD V8.1 (READ/WRITE + ANTI-BSOD) ---
VOID CommunicationThread(PVOID StartContext) {
    UNREFERENCED_PARAMETER(StartContext);

    LARGE_INTEGER Timeout;
    Timeout.QuadPart = -100000;
    KeDelayExecutionThread(KernelMode, FALSE, &Timeout);
    DbgPrint("[NIOH] Thread V8.1 (Safe Mode) gestartet.\n");

    HANDLE SectionHandle = NULL;
    PSHARED_MEMORY SharedMemory = NULL;
    SIZE_T viewSize = 0;
    UNICODE_STRING sectionName;
    OBJECT_ATTRIBUTES oa;

    RtlInitUnicodeString(&sectionName, L"\\BaseNamedObjects\\NiohSharedV6");
    InitializeObjectAttributes(&oa, &sectionName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    while (g_Running) {
        if (SharedMemory == NULL) {
            NTSTATUS status = ZwOpenSection(&SectionHandle, SECTION_ALL_ACCESS, &oa);
            if (NT_SUCCESS(status)) {
                status = ZwMapViewOfSection(SectionHandle, ZwCurrentProcess(),
                    (PVOID*)&SharedMemory, 0, sizeof(SHARED_MEMORY), NULL, &viewSize, ViewUnmap, 0, PAGE_READWRITE);
                if (NT_SUCCESS(status) && SharedMemory) {
                    DbgPrint("[NIOH] V8.1 Verbunden!\n");
                    SharedMemory->Status = 88;
                }
                else {
                    if (SectionHandle) ZwClose(SectionHandle);
                    SectionHandle = NULL;
                }
            }
        }
        else {
            if (MmIsAddressValid(SharedMemory)) {
                // PING
                if (SharedMemory->Status == 99) {
                    SharedMemory->Status = 100;
                }
                // BEFEHL EMPFANGEN (1=Read, 3=Write)
                else if (SharedMemory->Status == 1 || SharedMemory->Status == 3) {

                    // --- ANTI-BSOD SCHUTZ (Zurück aus V7) ---
                    // Wir prüfen die Zieladresse, BEVOR wir irgendwas tun.
                    if (SharedMemory->Address < 0x10000) {
                        // Zu klein (Nullpointer Schutz)
                        SharedMemory->Status = 2;
                    }
                    else if (SharedMemory->Address >= 0x7FFFFFFFFFFF) {
                        // Zu groß (Kernel-Space Schutz - NIEMALS dort schreiben!)
                        SharedMemory->Status = 2;
                    }
                    else {
                        // Adresse sieht sicher aus -> Ausführen
                        PEPROCESS Process = NULL;
                        SIZE_T BytesCopied = 0;

                        if (NT_SUCCESS(PsLookupProcessByProcessId((HANDLE)SharedMemory->ProcessId, &Process))) {

                            // READ (Game -> Wir)
                            if (SharedMemory->Status == 1) {
                                MmCopyVirtualMemory(Process, (PVOID)SharedMemory->Address, PsGetCurrentProcess(),
                                    SharedMemory->Buffer, SharedMemory->Size, KernelMode, &BytesCopied);
                            }
                            // WRITE (Wir -> Game)
                            else if (SharedMemory->Status == 3) {
                                MmCopyVirtualMemory(PsGetCurrentProcess(), SharedMemory->Buffer, Process,
                                    (PVOID)SharedMemory->Address, SharedMemory->Size, KernelMode, &BytesCopied);
                            }

                            ObDereferenceObject(Process);
                        }
                        SharedMemory->Status = 2; // Fertig
                    }
                }
            }
            else {
                // Shared Memory verloren/ungültig
                SharedMemory = NULL;
                if (SectionHandle) ZwClose(SectionHandle);
                SectionHandle = NULL;
            }
        }
        LARGE_INTEGER Interval;
        Interval.QuadPart = -1000;
        KeDelayExecutionThread(KernelMode, FALSE, &Interval);
    }

    if (SharedMemory) ZwUnmapViewOfSection(ZwCurrentProcess(), SharedMemory);
    if (SectionHandle) ZwClose(SectionHandle);
    PsTerminateSystemThread(STATUS_SUCCESS);
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    UNREFERENCED_PARAMETER(DriverObject);
    UNREFERENCED_PARAMETER(RegistryPath);
    DbgPrint("[NIOH] Treiber V6 Host-Mode.\n");
    NTSTATUS status = PsCreateSystemThread(&g_ThreadHandle, THREAD_ALL_ACCESS, NULL, NULL, NULL, CommunicationThread, NULL);
    if (!NT_SUCCESS(status)) return status;
    ZwClose(g_ThreadHandle);
    return STATUS_SUCCESS;
}