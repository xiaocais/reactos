
#include <internal/mmhal.h>
#include <internal/halio.h>
#include <internal/hal/page.h>
#include <ddk/ntddk.h>
#include <string.h>
#include <internal/string.h>
#include <defines.h>
#include <ddk/ntddblue.h>

//#define NDEBUG
#include <internal/debug.h>

#define IDMAP_BASE         (0xd0000000)
#define VIDMEM_BASE        0xb8000

/*
 * Return a linear address which can be used to access the physical memory
 * starting at x 
 */
extern inline unsigned int physical_to_linear(unsigned int x)
{
        return(x+IDMAP_BASE);
}

extern inline unsigned int linear_to_physical(unsigned int x)
{
        return(x-IDMAP_BASE);
}

#define NR_ROWS            50
#define NR_COLUMNS         80

#define CRTC_COMMAND       0x3d4
#define CRTC_DATA          0x3d5

#define CRTC_CURSORSTART   0x0a
#define CRTC_CURSOREND     0x0b
#define CRTC_CURSORPOSLO   0x0f
#define CRTC_CURSORPOSHI   0x0e




#define TAB_WIDTH          8


/* NOTES ******************************************************************/
/*
 *  [[character][attribute]][[character][attribute]]....
 */


/* TYPEDEFS ***************************************************************/

typedef struct _DEVICE_EXTENSION
{
    PBYTE VideoMemory;
    SHORT CursorX;
    SHORT CursorY;
    DWORD CursorSize;
    BOOL  CursorVisible;
    WORD  CharAttribute;
    DWORD Mode;
    BYTE  ScanLines;      /* Height of a text line */

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;





/* FUNCTIONS **************************************************************/


NTSTATUS ScrCreate (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
//    PIO_STACK_LOCATION stk = IoGetCurrentIrpStackLocation (Irp);
    PDEVICE_EXTENSION DeviceExtension;
    NTSTATUS Status;
    unsigned int offset;

    DeviceExtension = DeviceObject->DeviceExtension;

    /* initialize device extension */
    /* get pointer to video memory */
    DeviceExtension->VideoMemory = (PBYTE)physical_to_linear (VIDMEM_BASE);

    /* get current output position */
    __asm__("cli\n\t");
    outb_p (CRTC_COMMAND, CRTC_CURSORPOSLO);
    offset = inb_p (CRTC_DATA);
    outb_p (CRTC_COMMAND, CRTC_CURSORPOSHI);
    offset += (inb_p (CRTC_DATA) << 8);
    __asm__("sti\n\t");

    DeviceExtension->CursorX = (SHORT)(offset % NR_COLUMNS);
    DeviceExtension->CursorY = (SHORT)(offset / NR_COLUMNS);
    DeviceExtension->CursorSize    = 5; /* FIXME: value correct?? */
    DeviceExtension->CursorVisible = TRUE;


    DeviceExtension->ScanLines = 8; /* FIXME: read it from CRTC */


    /* more initialization */
    DeviceExtension->CharAttribute = 0x17;  /* light grey on blue */
    DeviceExtension->Mode = ENABLE_PROCESSED_OUTPUT |
                            ENABLE_WRAP_AT_EOL_OUTPUT;

    /* FIXME: more initialization?? */


    /* show blinking cursor */
    /* FIXME: calculate cursor size */
    __asm__("cli\n\t");
    outb_p (CRTC_COMMAND, CRTC_CURSORSTART);
    outb_p (CRTC_DATA, 0x47);
    outb_p (CRTC_COMMAND, CRTC_CURSOREND);
    outb_p (CRTC_DATA, 0x07);
    __asm__("sti\n\t");


    Status = STATUS_SUCCESS;
 
    Irp->IoStatus.Status = Status;
    IoCompleteRequest (Irp, IO_NO_INCREMENT);

    return (Status);
}


NTSTATUS ScrWrite (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PIO_STACK_LOCATION stk = IoGetCurrentIrpStackLocation (Irp);
    PDEVICE_EXTENSION DeviceExtension;
    NTSTATUS Status;
    char *pch = Irp->UserBuffer;
    char *vidmem;
    int i, j, offset;
    int cursorx, cursory;

    DeviceExtension = DeviceObject->DeviceExtension;
    vidmem  = DeviceExtension->VideoMemory;
    cursorx = DeviceExtension->CursorX;
    cursory = DeviceExtension->CursorY;

    for (i = 0; i < stk->Parameters.Write.Length; i++, pch++)
    {
        switch (*pch)
        {
            case '\b':
                if (cursorx > 0)
                {
                    cursorx--;
                }
                else if (cursory > 0)
                {
                    cursorx = NR_COLUMNS - 1;
                    cursory--;
                }   
                break;

            case '\n':
                cursory++;
                cursorx = 0;
                break;

            case '\r':
                break;

            case '\t':
                offset = TAB_WIDTH - (cursorx % TAB_WIDTH);
                for (j = 0; j < offset; j++)
                {
                    vidmem[(cursorx * 2) + (cursory * NR_COLUMNS * 2)] = ' ';
                    cursorx++;
/*
                    if (cursorx >= NR_COLUMNS)
                    {
                        cursory++;
                        cursorx = 0;
                    }
*/
                }
                break;
	
            default:
                vidmem[(cursorx * 2) + (cursory * NR_COLUMNS * 2)] = *pch;
                vidmem[(cursorx * 2) + (cursory * NR_COLUMNS * 2) + 1] = (char) DeviceExtension->CharAttribute;
                cursorx++;
                if (cursorx >= NR_COLUMNS)
                {
                    cursory++;
                    cursorx = 0;
                }
        }
   
   
        if (cursory >= NR_ROWS)
        {
            unsigned short *LinePtr;

            memcpy (vidmem, 
                    &vidmem[NR_COLUMNS * 2], 
                    NR_COLUMNS * (NR_ROWS - 1) * 2);

            LinePtr = (unsigned short *) &vidmem[NR_COLUMNS * (NR_ROWS - 1) * 2];

            for (j = 0; j < NR_COLUMNS; j++)
            {
                LinePtr[j] = DeviceExtension->CharAttribute << 8;
            }
            cursory = NR_ROWS - 1;
        }
    }


    /* Set the cursor position */
  
    offset = (cursory * NR_COLUMNS) + cursorx;
   
    outb_p (CRTC_COMMAND, CRTC_CURSORPOSLO);
    outb_p (CRTC_DATA, offset);
    outb_p (CRTC_COMMAND, CRTC_CURSORPOSHI);
    offset >>= 8;
    outb_p (CRTC_DATA, offset);

    DeviceExtension->CursorX = cursorx;
    DeviceExtension->CursorY = cursory;


    Status = STATUS_SUCCESS;
   
    Irp->IoStatus.Status = Status;
    IoCompleteRequest (Irp, IO_NO_INCREMENT);

    return (Status);
}




NTSTATUS ScrIoControl (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PIO_STACK_LOCATION stk = IoGetCurrentIrpStackLocation (Irp);
    PDEVICE_EXTENSION DeviceExtension;
    NTSTATUS Status;


    DeviceExtension = DeviceObject->DeviceExtension;

    switch (stk->Parameters.DeviceIoControl.IoControlCode)
    {
        case IOCTL_CONSOLE_GET_SCREEN_BUFFER_INFO:
            {
                PCONSOLE_SCREEN_BUFFER_INFO pcsbi = (PCONSOLE_SCREEN_BUFFER_INFO)Irp->AssociatedIrp.SystemBuffer;
                unsigned int offset;

                __asm__("cli\n\t");
                outb_p (CRTC_COMMAND, CRTC_CURSORPOSLO);
                offset = inb_p (CRTC_DATA);
                outb_p (CRTC_COMMAND, CRTC_CURSORPOSHI);
                offset += (inb_p (CRTC_DATA) << 8);
                __asm__("sti\n\t");

                pcsbi->dwCursorPosition.X = (SHORT)(offset % NR_COLUMNS);
                pcsbi->dwCursorPosition.Y = (SHORT)(offset / NR_COLUMNS);

                pcsbi->dwSize.X = NR_ROWS;
                pcsbi->dwSize.Y = NR_COLUMNS;

                Irp->IoStatus.Information = sizeof (CONSOLE_SCREEN_BUFFER_INFO);
                Status = STATUS_SUCCESS;
            }
            break;
           
        case IOCTL_CONSOLE_SET_SCREEN_BUFFER_INFO:
            {
                PCONSOLE_SCREEN_BUFFER_INFO pcsbi = (PCONSOLE_SCREEN_BUFFER_INFO)Irp->AssociatedIrp.SystemBuffer;
                unsigned int offset;

                DeviceExtension->CursorX = pcsbi->dwCursorPosition.X;
                DeviceExtension->CursorY = pcsbi->dwCursorPosition.Y;

                offset = (pcsbi->dwCursorPosition.Y * NR_COLUMNS) +
                          pcsbi->dwCursorPosition.X;

                __asm__("cli\n\t");
                outb_p (CRTC_COMMAND, CRTC_CURSORPOSLO);
                outb_p (CRTC_DATA, offset);
                outb_p (CRTC_COMMAND, CRTC_CURSORPOSHI);
                outb_p (CRTC_DATA, offset>>8);
                __asm__("sti\n\t");

                Irp->IoStatus.Information = 0;
                Status = STATUS_SUCCESS;
            }
            break;

        case IOCTL_CONSOLE_GET_CURSOR_INFO:
            {
//               PCONSOLE_CURSOR_INFO pcci = (PCONSOLE_CURSOR_INFO)Irp->AssociatedIrp.SystemBuffer;

//               DPRINT("GetConsoleCursorInfo: pcci=%p\n", pcci);
//               DPRINT("GetConsoleCursorInfo\n");
//               pcci->dwSize = 10;
//               pcci->bVisible = FALSE;

                Irp->IoStatus.Information = 0;
                Status = STATUS_SUCCESS;
            }
            break;

        case IOCTL_CONSOLE_SET_CURSOR_INFO:
            {
                PCONSOLE_CURSOR_INFO pcci = (PCONSOLE_CURSOR_INFO)Irp->AssociatedIrp.SystemBuffer;
                BYTE data;
                DWORD size, height;

                DeviceExtension->CursorSize = pcci->dwSize;
                DeviceExtension->CursorVisible = pcci->bVisible;
                height = DeviceExtension->ScanLines;
                data = (pcci->bVisible) ? 0x40 : 0x20;

                size = (pcci->dwSize * height) / 100;
                if (size < 1)
                    size = 1;

                data |= (BYTE)(height - size);

                __asm__("cli\n\t");
                outb_p (CRTC_COMMAND, CRTC_CURSORSTART);
                outb_p (CRTC_DATA, data);
                outb_p (CRTC_COMMAND, CRTC_CURSOREND);
                outb_p (CRTC_DATA, height - 1);
                __asm__("sti\n\t");

                Irp->IoStatus.Information = 0;
                Status = STATUS_SUCCESS;
            }
            break;



        case IOCTL_CONSOLE_FILL_OUTPUT_ATTRIBUTE:
            {
                PFILL_OUTPUT_ATTRIBUTE Buf = (PFILL_OUTPUT_ATTRIBUTE)Irp->AssociatedIrp.SystemBuffer;
                char *vidmem;
                int offset;
                DWORD dwCount;

                vidmem = DeviceExtension->VideoMemory;
                offset = (DeviceExtension->CursorY * NR_COLUMNS * 2) +
                         (DeviceExtension->CursorX * 2) + 1;

                CHECKPOINT

                for (dwCount = 0; dwCount < Buf->nLength; dwCount++)
                {
                    vidmem[offset + (dwCount * 2)] = (char) Buf->wAttribute;
                }
                
                Irp->IoStatus.Information = 0;
                Status = STATUS_SUCCESS;
            }
            break;

#if 0
        case IOCTL_CONSOLE_READ_OUTPUT_ATTRIBUTE:
        case IOCTL_CONSOLE_WRITE_OUTPUT_ATTRIBUTE:
            break;
#endif

        case IOCTL_CONSOLE_SET_TEXT_ATTRIBUTE:
            DeviceExtension->CharAttribute = (WORD)Irp->AssociatedIrp.SystemBuffer;
            Irp->IoStatus.Information = 0;
            Status = STATUS_SUCCESS;
            break;


        case IOCTL_CONSOLE_FILL_OUTPUT_CHARACTER:
            {
                PFILL_OUTPUT_CHARACTER Buf = (PFILL_OUTPUT_CHARACTER)Irp->AssociatedIrp.SystemBuffer;
                char *vidmem;
                int offset;
                DWORD dwCount;

                vidmem = DeviceExtension->VideoMemory;
                offset = (DeviceExtension->CursorY * NR_COLUMNS * 2) +
                         (DeviceExtension->CursorX * 2);

                CHECKPOINT

                for (dwCount = 0; dwCount < Buf->nLength; dwCount++)
                {
                    vidmem[offset + (dwCount * 2)] = (char) Buf->cCharacter;
                }

                Irp->IoStatus.Information = 0;
                Status = STATUS_SUCCESS;
            }
            break;



        default:
            Status = STATUS_NOT_IMPLEMENTED;
    }

    Irp->IoStatus.Status = Status;
    IoCompleteRequest (Irp, IO_NO_INCREMENT);

    return (Status);
}






VOID ScrStartIo(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
//   PIO_STACK_LOCATION stk = IoGetCurrentIrpStackLocation(Irp);
    
}


NTSTATUS ScrDispatch (PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PIO_STACK_LOCATION stk = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS Status;

    switch (stk->MajorFunction)
    {
        case IRP_MJ_CLOSE:
            Status = STATUS_SUCCESS;
            break;

        default:
            Status = STATUS_NOT_IMPLEMENTED;
            break;
    }


    Irp->IoStatus.Status = Status;
    IoCompleteRequest (Irp, IO_NO_INCREMENT);

    return (Status);
}


/*
 * Module entry point
 */
STDCALL NTSTATUS
DriverEntry (PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    PDEVICE_OBJECT DeviceObject;
    ANSI_STRING adevice_name;
    UNICODE_STRING device_name;
    ANSI_STRING asymlink_name;
    UNICODE_STRING symlink_name;
   
    DbgPrint ("Screen Driver 0.0.5\n");

    DriverObject->MajorFunction[IRP_MJ_CREATE] = ScrCreate;
    DriverObject->MajorFunction[IRP_MJ_CLOSE]  = ScrDispatch;
    DriverObject->MajorFunction[IRP_MJ_READ]   = ScrDispatch;
    DriverObject->MajorFunction[IRP_MJ_WRITE]  = ScrWrite;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL ] = ScrIoControl;
    DriverObject->DriverStartIo = ScrStartIo;

    RtlInitAnsiString (&adevice_name, "\\Device\\BlueScreen");
    RtlAnsiStringToUnicodeString (&device_name, &adevice_name, TRUE);
    IoCreateDevice (DriverObject,
                    sizeof(DEVICE_EXTENSION),
                    &device_name,
                    FILE_DEVICE_SCREEN,
                    0,
                    TRUE,
                    &DeviceObject);

    RtlInitAnsiString (&asymlink_name, "\\??\\BlueScreen");
    RtlAnsiStringToUnicodeString (&symlink_name, &asymlink_name, TRUE);
    IoCreateSymbolicLink (&symlink_name, &device_name);

    return (STATUS_SUCCESS);
}

