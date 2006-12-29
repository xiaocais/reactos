/* $Id$
 *
 * COPYRIGHT:       See COPYING in the top directory
 * PROJECT:         ReactOS kernel
 * FILE:            ntoskrnl/mm/mm.c
 * PURPOSE:         Kernel memory managment functions
 * PROGRAMMERS:     David Welch (welch@cwcom.net)
 */

/* INCLUDES *****************************************************************/

#include <ntoskrnl.h>
#define NDEBUG
#include <internal/debug.h>

/* GLOBALS *****************************************************************/

ULONG MmUserProbeAddress = 0;
PVOID MmHighestUserAddress = NULL;
PBOOLEAN Mm64BitPhysicalAddress = FALSE;
PVOID MmSystemRangeStart = NULL;
ULONG MmReadClusterSize;

MM_STATS MmStats;

/* FUNCTIONS ****************************************************************/

/*
 * @implemented
 */
BOOLEAN STDCALL MmIsNonPagedSystemAddressValid(PVOID VirtualAddress)
{
   return MmIsAddressValid(VirtualAddress);
}

/*
 * @implemented
 */
BOOLEAN STDCALL MmIsAddressValid(PVOID VirtualAddress)
/*
 * FUNCTION: Checks whether the given address is valid for a read or write
 * ARGUMENTS:
 *          VirtualAddress = address to check
 * RETURNS: True if the access would be valid
 *          False if the access would cause a page fault
 * NOTES: This function checks whether a byte access to the page would
 *        succeed. Is this realistic for RISC processors which don't
 *        allow byte granular access?
 */
{
   MEMORY_AREA* MemoryArea;
   PMADDRESS_SPACE AddressSpace;

   if (VirtualAddress >= MmSystemRangeStart)
   {
      AddressSpace = MmGetKernelAddressSpace();
   }
   else
   {
      AddressSpace = (PMADDRESS_SPACE)&(PsGetCurrentProcess())->VadRoot;
   }

   MmLockAddressSpace(AddressSpace);
   MemoryArea = MmLocateMemoryAreaByAddress(AddressSpace,
                                            VirtualAddress);

   if (MemoryArea == NULL || MemoryArea->DeleteInProgress)
   {
      MmUnlockAddressSpace(AddressSpace);
      return(FALSE);
   }
   MmUnlockAddressSpace(AddressSpace);
   return(TRUE);
}

NTSTATUS
NTAPI
MmpAccessFault(KPROCESSOR_MODE Mode,
                  ULONG_PTR Address,
                  BOOLEAN FromMdl)
{
   PMADDRESS_SPACE AddressSpace;
   MEMORY_AREA* MemoryArea;
   NTSTATUS Status;
   BOOLEAN Locked = FromMdl;

   DPRINT("MmAccessFault(Mode %d, Address %x)\n", Mode, Address);

   if (KeGetCurrentIrql() >= DISPATCH_LEVEL)
   {
      CPRINT("Page fault at high IRQL was %d\n", KeGetCurrentIrql());
      return(STATUS_UNSUCCESSFUL);
   }
   if (PsGetCurrentProcess() == NULL)
   {
      DPRINT("No current process\n");
      return(STATUS_UNSUCCESSFUL);
   }

   /*
    * Find the memory area for the faulting address
    */
   if (Address >= (ULONG_PTR)MmSystemRangeStart)
   {
      /*
       * Check permissions
       */
      if (Mode != KernelMode)
      {
         DPRINT1("MmAccessFault(Mode %d, Address %x)\n", Mode, Address);
         DbgPrint("%s:%d\n",__FILE__,__LINE__);
         return(STATUS_ACCESS_VIOLATION);
      }
      AddressSpace = MmGetKernelAddressSpace();
   }
   else
   {
      AddressSpace = (PMADDRESS_SPACE)&(PsGetCurrentProcess())->VadRoot;
   }

   if (!FromMdl)
   {
      MmLockAddressSpace(AddressSpace);
   }
   do
   {
      MemoryArea = MmLocateMemoryAreaByAddress(AddressSpace, (PVOID)Address);
      if (MemoryArea == NULL || MemoryArea->DeleteInProgress)
      {
         if (!FromMdl)
         {
            MmUnlockAddressSpace(AddressSpace);
         }
         return (STATUS_ACCESS_VIOLATION);
      }

      switch (MemoryArea->Type)
      {
         case MEMORY_AREA_SYSTEM:
            Status = STATUS_ACCESS_VIOLATION;
            break;

         case MEMORY_AREA_PAGED_POOL:
            Status = STATUS_SUCCESS;
            break;

         case MEMORY_AREA_SECTION_VIEW:
            Status = MmAccessFaultSectionView(AddressSpace,
                                              MemoryArea,
                                              (PVOID)Address,
                                              Locked);
            break;

         case MEMORY_AREA_VIRTUAL_MEMORY:
            Status = STATUS_ACCESS_VIOLATION;
            break;

         case MEMORY_AREA_SHARED_DATA:
            Status = STATUS_ACCESS_VIOLATION;
            break;

         default:
            Status = STATUS_ACCESS_VIOLATION;
            break;
      }
   }
   while (Status == STATUS_MM_RESTART_OPERATION);

   DPRINT("Completed page fault handling\n");
   if (!FromMdl)
   {
      MmUnlockAddressSpace(AddressSpace);
   }
   return(Status);
}

NTSTATUS
NTAPI
MmNotPresentFault(KPROCESSOR_MODE Mode,
                           ULONG_PTR Address,
                           BOOLEAN FromMdl)
{
   PMADDRESS_SPACE AddressSpace;
   MEMORY_AREA* MemoryArea;
   NTSTATUS Status;
   BOOLEAN Locked = FromMdl;
   PFN_TYPE Pfn;

   DPRINT("MmNotPresentFault(Mode %d, Address %x)\n", Mode, Address);

   if (KeGetCurrentIrql() >= DISPATCH_LEVEL)
   {
      CPRINT("Page fault at high IRQL was %d, address %x\n", KeGetCurrentIrql(), Address);
      return(STATUS_UNSUCCESSFUL);
   }
   if (PsGetCurrentProcess() == NULL)
   {
      /* Allow this! It lets us page alloc much earlier! It won't be needed 
       * after my init patch anyways
       */
      DPRINT("No current process\n");
      if (Address < (ULONG_PTR)MmSystemRangeStart)
      {
         return(STATUS_ACCESS_VIOLATION);
      }
   }

   /*
    * Find the memory area for the faulting address
    */
   if (Address >= (ULONG_PTR)MmSystemRangeStart)
   {
      /*
       * Check permissions
       */
      if (Mode != KernelMode)
      {
	 CPRINT("Address: %x\n", Address);
         return(STATUS_ACCESS_VIOLATION);
      }
      AddressSpace = MmGetKernelAddressSpace();
   }
   else
   {
      AddressSpace = (PMADDRESS_SPACE)&(PsGetCurrentProcess())->VadRoot;
   }

   if (!FromMdl)
   {
      MmLockAddressSpace(AddressSpace);
   }

   /*
    * Call the memory area specific fault handler
    */
   do
   {
      MemoryArea = MmLocateMemoryAreaByAddress(AddressSpace, (PVOID)Address);
      if (MemoryArea == NULL || MemoryArea->DeleteInProgress)
      {
         if (!FromMdl)
         {
            MmUnlockAddressSpace(AddressSpace);
         }
         return (STATUS_ACCESS_VIOLATION);
      }

      switch (MemoryArea->Type)
      {
         case MEMORY_AREA_PAGED_POOL:
            {
               Status = MmCommitPagedPoolAddress((PVOID)Address, Locked);
               break;
            }

         case MEMORY_AREA_SYSTEM:
            Status = STATUS_ACCESS_VIOLATION;
            break;

         case MEMORY_AREA_SECTION_VIEW:
            Status = MmNotPresentFaultSectionView(AddressSpace,
                                                  MemoryArea,
                                                  (PVOID)Address,
                                                  Locked);
            break;

         case MEMORY_AREA_VIRTUAL_MEMORY:
         case MEMORY_AREA_PEB_OR_TEB:
            Status = MmNotPresentFaultVirtualMemory(AddressSpace,
                                                    MemoryArea,
                                                    (PVOID)Address,
                                                    Locked);
            break;

         case MEMORY_AREA_SHARED_DATA:
	    Pfn = MmSharedDataPagePhysicalAddress.QuadPart >> PAGE_SHIFT;
            Status =
               MmCreateVirtualMapping(PsGetCurrentProcess(),
                                      (PVOID)PAGE_ROUND_DOWN(Address),
                                      PAGE_READONLY,
                                      &Pfn,
                                      1);
            break;

         default:
            Status = STATUS_ACCESS_VIOLATION;
            break;
      }
   }
   while (Status == STATUS_MM_RESTART_OPERATION);

   DPRINT("Completed page fault handling\n");
   if (!FromMdl)
   {
      MmUnlockAddressSpace(AddressSpace);
   }
   return(Status);
}

extern BOOLEAN Mmi386MakeKernelPageTableGlobal(PVOID Address);

NTSTATUS
NTAPI
MmAccessFault(IN BOOLEAN StoreInstruction,
              IN PVOID Address,
              IN KPROCESSOR_MODE Mode,
              IN PVOID TrapInformation)
{
    /* Cute little hack for ROS */
    if ((ULONG_PTR)Address >= (ULONG_PTR)MmSystemRangeStart)
    {
        /* Check for an invalid page directory in kernel mode */
        if (Mmi386MakeKernelPageTableGlobal(Address))
        {
            /* All is well with the world */
            return STATUS_SUCCESS;
        }
    }

    /* Keep same old ReactOS Behaviour */
    if (StoreInstruction)
    {
        /* Call access fault */
        return MmpAccessFault(Mode, (ULONG_PTR)Address, TrapInformation ? FALSE : TRUE);
    }
    else
    {
        /* Call not present */
        return MmNotPresentFault(Mode, (ULONG_PTR)Address, TrapInformation ? FALSE : TRUE);
    }
}

NTSTATUS
NTAPI
MmCommitPagedPoolAddress(PVOID Address, BOOLEAN Locked)
{
   NTSTATUS Status;
   PFN_TYPE AllocatedPage;
   Status = MmRequestPageMemoryConsumer(MC_PPOOL, FALSE, &AllocatedPage);
   if (!NT_SUCCESS(Status))
   {
      MmUnlockAddressSpace(MmGetKernelAddressSpace());
      Status = MmRequestPageMemoryConsumer(MC_PPOOL, TRUE, &AllocatedPage);
      MmLockAddressSpace(MmGetKernelAddressSpace());
   }
   Status =
      MmCreateVirtualMapping(NULL,
                             (PVOID)PAGE_ROUND_DOWN(Address),
                             PAGE_READWRITE,
                             &AllocatedPage,
                             1);
   if (Locked)
   {
      MmLockPage(AllocatedPage);
   }
   return(Status);
}



/* Miscellanea functions: they may fit somewhere else */

/*
 * @unimplemented
 */
ULONG STDCALL
MmAdjustWorkingSetSize (ULONG Unknown0,
                        ULONG Unknown1,
                        ULONG Unknown2)
{
   UNIMPLEMENTED;
   return (0);
}


ULONG
STDCALL
MmDbgTranslatePhysicalAddress (
   ULONG Unknown0,
   ULONG Unknown1
)
{
   UNIMPLEMENTED;
   return (0);
}

/*
 * @unimplemented
 */
BOOLEAN
STDCALL
MmSetAddressRangeModified (
    IN PVOID    Address,
    IN ULONG    Length
)
{
   UNIMPLEMENTED;
   return (FALSE);
}

/*
 * @implemented
 */
PVOID
NTAPI
MmGetSystemRoutineAddress(IN PUNICODE_STRING SystemRoutineName)
{
    PVOID ProcAddress;
    ANSI_STRING AnsiRoutineName;
    NTSTATUS Status;
    PLIST_ENTRY NextEntry;
    extern LIST_ENTRY ModuleListHead;
    PLDR_DATA_TABLE_ENTRY LdrEntry;
    BOOLEAN Found = FALSE;
    UNICODE_STRING KernelName = RTL_CONSTANT_STRING(L"ntoskrnl.exe");
    UNICODE_STRING HalName = RTL_CONSTANT_STRING(L"hal.dll");

    /* Convert routine to ansi name */
    Status = RtlUnicodeStringToAnsiString(&AnsiRoutineName,
                                          SystemRoutineName,
                                          TRUE);
    if (!NT_SUCCESS(Status)) return NULL;

    /* Loop the loaded module list */
    NextEntry = ModuleListHead.Flink;
    while (NextEntry != &ModuleListHead)
    {
        /* Get the entry */
        LdrEntry = CONTAINING_RECORD(NextEntry,
                                     LDR_DATA_TABLE_ENTRY,
                                     InLoadOrderLinks);

        /* Check if it's the kernel or HAL */
        if (RtlEqualUnicodeString(&KernelName, &LdrEntry->BaseDllName, TRUE))
        {
            /* Found it */
            Found = TRUE;
        }
        else if (RtlEqualUnicodeString(&HalName, &LdrEntry->BaseDllName, TRUE))
        {
            /* Found it */
            Found = TRUE;
        }

        /* Check if we found a valid binary */
        if (Found)
        {
            /* Find the procedure name */
            Status = LdrGetProcedureAddress(LdrEntry->DllBase,
                                            &AnsiRoutineName,
                                            0,
                                            &ProcAddress);
            break;
        }

        /* Keep looping */
        NextEntry = NextEntry->Flink;
    }

    /* Free the string and return */
    RtlFreeAnsiString(&AnsiRoutineName);
    return (NT_SUCCESS(Status) ? ProcAddress : NULL);
}

NTSTATUS
NTAPI
NtGetWriteWatch(IN HANDLE ProcessHandle,
                IN ULONG Flags,
                IN PVOID BaseAddress,
                IN ULONG RegionSize,
                IN PVOID *UserAddressArray,
                OUT PULONG EntriesInUserAddressArray,
                OUT PULONG Granularity)
{
    UNIMPLEMENTED;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
NTAPI
NtResetWriteWatch(IN HANDLE ProcessHandle,
                 IN PVOID BaseAddress,
                 IN ULONG RegionSize)
{
    UNIMPLEMENTED;
    return STATUS_NOT_IMPLEMENTED;
}

/* EOF */
