/*
 * COPYRIGHT:       GPL - See COPYING in the top level directory
 * PROJECT:         ReactOS Virtual DOS Machine
 * FILE:            dos/dos32krnl/condrv.c
 * PURPOSE:         DOS32 CON Driver
 * PROGRAMMERS:     Aleksandar Andrejevic <theflash AT sdf DOT lonestar DOT org>
 *                  Hermes Belusca-Maito (hermes.belusca@sfr.fr)
 */

/* INCLUDES *******************************************************************/

#define NDEBUG

#include "emulator.h"

#include "dos.h"
#include "dos/dem.h"

#include "bios/bios.h"

/* PRIVATE VARIABLES **********************************************************/

PDOS_DEVICE_NODE ConIn = NULL, ConOut = NULL;

/* PRIVATE FUNCTIONS **********************************************************/

WORD NTAPI ConDrvReadInput(PDOS_DEVICE_NODE Device, DWORD Buffer, PWORD Length)
{
    CHAR Character;
    WORD BytesRead;
    PCHAR Pointer = (PCHAR)FAR_POINTER(Buffer);

    /* Save AX */
    USHORT AX = getAX();

    /*
     * Use BIOS Get Keystroke function
     */
    for (BytesRead = 0; BytesRead < *Length; BytesRead++)
    {
        /* Call the BIOS INT 16h, AH=00h "Get Keystroke" */
        setAH(0x00);
        Int32Call(&DosContext, BIOS_KBD_INTERRUPT);

        /* Retrieve the character in AL (scan code is in AH) */
        Character = getAL();

        if (DoEcho) DosPrintCharacter(DOS_OUTPUT_HANDLE, Character);
        Pointer[BytesRead] = Character;

        /* Stop on first carriage return */
        if (Character == '\r')
        {
            if (DoEcho) DosPrintCharacter(DOS_OUTPUT_HANDLE, '\n');
            break;
        }
    }

    *Length = BytesRead;

    /* Restore AX */
    setAX(AX);
    return DOS_DEVSTAT_DONE;
}

WORD NTAPI ConDrvInputStatus(PDOS_DEVICE_NODE Device)
{
    /* Save AX */
    USHORT AX = getAX();

    /* Call the BIOS */
    setAH(0x01); // or 0x11 for enhanced, but what to choose?
    Int32Call(&DosContext, BIOS_KBD_INTERRUPT);

    /* Restore AX */
    setAX(AX);

    /* If ZF is set, set the busy bit */
    if (getZF()) return DOS_DEVSTAT_BUSY;
    else return DOS_DEVSTAT_DONE;
}

WORD NTAPI ConDrvWriteOutput(PDOS_DEVICE_NODE Device, DWORD Buffer, PWORD Length)
{
    WORD BytesWritten;
    PCHAR Pointer = (PCHAR)FAR_POINTER(Buffer);

    /*
     * Use BIOS Teletype function
     */

    /* Save AX and BX */
    USHORT AX = getAX();
    USHORT BX = getBX();

    // FIXME: Use BIOS Write String function INT 10h, AH=13h ??
    for (BytesWritten = 0; BytesWritten < *Length; BytesWritten++)
    {
        /* Set the parameters */
        setAL(Pointer[BytesWritten]);
        setBL(DOS_CHAR_ATTRIBUTE);
        setBH(Bda->VideoPage);

        /* Call the BIOS INT 10h, AH=0Eh "Teletype Output" */
        setAH(0x0E);
        Int32Call(&DosContext, BIOS_VIDEO_INTERRUPT);
    }

    /* Restore AX and BX */
    setBX(BX);
    setAX(AX);
    return DOS_DEVSTAT_DONE;
}

WORD NTAPI ConDrvOpen(PDOS_DEVICE_NODE Device)
{
    DPRINT("Handle to %Z opened\n", &Device->Name);
    return DOS_DEVSTAT_DONE;
}

WORD NTAPI ConDrvClose(PDOS_DEVICE_NODE Device)
{
    DPRINT("Handle to %Z closed\n", &Device->Name);
    return DOS_DEVSTAT_DONE;
}

/* PUBLIC FUNCTIONS ***********************************************************/

VOID ConDrvInitialize(PDOS_DEVICE_NODE *InputDevice, PDOS_DEVICE_NODE *OutputDevice)
{
    ConIn = DosCreateDevice(DOS_DEVATTR_STDIN
                            | DOS_DEVATTR_CON
                            | DOS_DEVATTR_CHARACTER,
                            "CONIN$");
    ConOut = DosCreateDevice(DOS_DEVATTR_STDOUT
                             | DOS_DEVATTR_CON
                             | DOS_DEVATTR_CHARACTER,
                             "CONOUT$");
    ASSERT(ConIn != NULL && ConOut != NULL);

    ConIn->ReadRoutine = ConDrvReadInput;
    ConIn->InputStatusRoutine = ConDrvInputStatus;
    ConOut->WriteRoutine = ConDrvWriteOutput;
    ConIn->OpenRoutine = ConOut->OpenRoutine = ConDrvOpen;
    ConIn->CloseRoutine = ConOut->CloseRoutine = ConDrvClose;

    if (InputDevice) *InputDevice = ConIn;
    if (OutputDevice) *OutputDevice = ConOut;
}

VOID ConDrvCleanup(VOID)
{
    if (ConIn) DosDeleteDevice(ConIn);
    if (ConOut) DosDeleteDevice(ConOut);
}
