/*
 * Configuration manager functions
 *
 * Copyright 2000 James Hatheway
 * Copyright 2005, 2006 Eric Kohl
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "setupapi_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(setupapi);

/* Registry key and value names */
static const WCHAR Backslash[] = {'\\', 0};
static const WCHAR Class[]  = {'C','l','a','s','s',0};

static const WCHAR ControlClass[] = {'S','y','s','t','e','m','\\',
                                     'C','u','r','r','e','n','t','C','o','n','t','r','o','l','S','e','t','\\',
                                     'C','o','n','t','r','o','l','\\',
                                     'C','l','a','s','s',0};

static const WCHAR DeviceClasses[] = {'S','y','s','t','e','m','\\',
                                      'C','u','r','r','e','n','t','C','o','n','t','r','o','l','S','e','t','\\',
                                      'C','o','n','t','r','o','l','\\',
                                      'D','e','v','i','c','e','C','l','a','s','s','e','s',0};

typedef struct _MACHINE_INFO
{
    WCHAR szMachineName[MAX_PATH];
    RPC_BINDING_HANDLE BindingHandle;
    HSTRING_TABLE StringTable;
} MACHINE_INFO, *PMACHINE_INFO;


static BOOL GuidToString(LPGUID Guid, LPWSTR String)
{
    LPWSTR lpString;

    if (UuidToStringW(Guid, &lpString) != RPC_S_OK)
        return FALSE;

    lstrcpyW(&String[1], lpString);

    String[0] = L'{';
    String[MAX_GUID_STRING_LEN - 2] = L'}';
    String[MAX_GUID_STRING_LEN - 1] = 0;

    RpcStringFree(&lpString);

    return TRUE;
}


/***********************************************************************
 * CMP_Init_Detection [SETUPAPI.@]
 */
CONFIGRET WINAPI CMP_Init_Detection(
    DWORD dwMagic)
{
    RPC_BINDING_HANDLE BindingHandle = NULL;

    TRACE("%lu\n", dwMagic);

    if (dwMagic != CMP_MAGIC)
        return CR_INVALID_DATA;

    if (!PnpGetLocalHandles(&BindingHandle, NULL))
        return CR_FAILURE;

    return PNP_InitDetection(BindingHandle);
}


/***********************************************************************
 * CMP_Report_LogOn [SETUPAPI.@]
 */
CONFIGRET WINAPI CMP_Report_LogOn(
    DWORD dwMagic,
    DWORD dwProcessId)
{
    RPC_BINDING_HANDLE BindingHandle = NULL;
    CONFIGRET ret = CR_SUCCESS;
    BOOL bAdmin;
    DWORD i;

    TRACE("%lu\n", dwMagic);

    if (dwMagic != CMP_MAGIC)
        return CR_INVALID_DATA;

    if (!PnpGetLocalHandles(&BindingHandle, NULL))
        return CR_FAILURE;

    bAdmin = IsUserAdmin();

    for (i = 0; i < 30; i++)
    {
        ret = PNP_ReportLogOn(BindingHandle,
                              bAdmin,
                              dwProcessId);
        if (ret == CR_SUCCESS)
            break;

        Sleep(5000);
    }

    return ret;
}


/***********************************************************************
 * CM_Connect_MachineA [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Connect_MachineA(
    PCSTR UNCServerName, PHMACHINE phMachine)
{
    PWSTR pServerNameW;
    CONFIGRET ret;

    TRACE("%s %p\n", UNCServerName, phMachine);

    if (UNCServerName == NULL || *UNCServerName == 0)
        return CM_Connect_MachineW(NULL, phMachine);

    if (CaptureAndConvertAnsiArg(UNCServerName, &pServerNameW))
        return CR_INVALID_DATA;

    ret = CM_Connect_MachineW(pServerNameW, phMachine);

    MyFree(pServerNameW);

    return ret;
}


/***********************************************************************
 * CM_Connect_MachineW [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Connect_MachineW(
    PCWSTR UNCServerName, PHMACHINE phMachine)
{
    PMACHINE_INFO pMachine;

    TRACE("%s %p\n", debugstr_w(UNCServerName), phMachine);

    pMachine = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(MACHINE_INFO));
    if (pMachine == NULL)
        return CR_OUT_OF_MEMORY;

    lstrcpyW(pMachine->szMachineName, UNCServerName);

    pMachine->StringTable = StringTableInitialize();
    if (pMachine->StringTable == NULL)
    {
        HeapFree(GetProcessHeap(), 0, pMachine);
        return CR_FAILURE;
    }

    StringTableAddString(pMachine->StringTable, L"PLT", 1);

    if (!PnpBindRpc(UNCServerName, &pMachine->BindingHandle))
    {
        StringTableDestroy(pMachine->StringTable);
        HeapFree(GetProcessHeap(), 0, pMachine);
        return CR_INVALID_MACHINENAME;
    }

    phMachine = (PHMACHINE)pMachine;

    return CR_SUCCESS;
}


/***********************************************************************
 * CM_Create_DevNodeA [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Create_DevNodeA(
    PDEVINST pdnDevInst, DEVINSTID_A pDeviceID, DEVINST dnParent,
    ULONG ulFlags)
{
    TRACE("%p %s %p %lx\n",
          pdnDevInst, debugstr_a(pDeviceID), dnParent, ulFlags);
    return CM_Create_DevNode_ExA(pdnDevInst, pDeviceID, dnParent,
                                 ulFlags, NULL);
}


/***********************************************************************
 * CM_Create_DevNodeW [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Create_DevNodeW(
    PDEVINST pdnDevInst, DEVINSTID_W pDeviceID, DEVINST dnParent,
    ULONG ulFlags)
{
    TRACE("%p %s %p %lx\n",
          pdnDevInst, debugstr_w(pDeviceID), dnParent, ulFlags);
    return CM_Create_DevNode_ExW(pdnDevInst, pDeviceID, dnParent,
                                 ulFlags, NULL);
}


/***********************************************************************
 * CM_Create_DevNode_ExA [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Create_DevNode_ExA(
    PDEVINST pdnDevInst, DEVINSTID_A pDeviceID, DEVINST dnParent,
    ULONG ulFlags, HANDLE hMachine)
{
    DEVINSTID_W pDeviceIDW;
    CONFIGRET ret;

    TRACE("%p %s %p %lx %p\n",
          pdnDevInst, debugstr_a(pDeviceID), dnParent, ulFlags, hMachine);

    if (CaptureAndConvertAnsiArg(pDeviceID, &pDeviceIDW))
        return CR_INVALID_DATA;

    ret = CM_Create_DevNode_ExW(pdnDevInst, pDeviceIDW, dnParent, ulFlags,
                                hMachine);

    MyFree(pDeviceIDW);

    return ret;
}


/***********************************************************************
 * CM_Create_DevNode_ExW [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Create_DevNode_ExW(
    PDEVINST pdnDevInst, DEVINSTID_W pDeviceID, DEVINST dnParent,
    ULONG ulFlags, HANDLE hMachine)
{
    RPC_BINDING_HANDLE BindingHandle = NULL;
    HSTRING_TABLE StringTable = NULL;
    LPWSTR lpParentDevInst;
    CONFIGRET ret = CR_SUCCESS;

    FIXME("%p %s %p %lx %p\n",
          pdnDevInst, debugstr_w(pDeviceID), dnParent, ulFlags, hMachine);

    if (!IsUserAdmin())
        return CR_ACCESS_DENIED;

    if (pdnDevInst == NULL)
        return CR_INVALID_POINTER;

    if (pDeviceID == NULL || wcslen(pDeviceID) == 0)
        return CR_INVALID_DEVICE_ID;

    if (dnParent == 0)
        return CR_INVALID_DEVNODE;

    if (ulFlags & ~CM_CREATE_DEVNODE_BITS)
        return CR_INVALID_FLAG;

    if (hMachine != NULL)
    {
        BindingHandle = ((PMACHINE_INFO)hMachine)->BindingHandle;
        if (BindingHandle == NULL)
            return CR_FAILURE;

        StringTable = ((PMACHINE_INFO)hMachine)->StringTable;
        if (StringTable == 0)
            return CR_FAILURE;
    }
    else
    {
        if (!PnpGetLocalHandles(&BindingHandle, &StringTable))
            return CR_FAILURE;
    }

    lpParentDevInst = StringTableStringFromId(StringTable, dnParent);
    if (lpParentDevInst == NULL)
        return CR_INVALID_DEVNODE;

    ret = PNP_CreateDevInst(BindingHandle,
                            pDeviceID,
                            lpParentDevInst,
                            MAX_DEVICE_ID_LEN,
                            ulFlags);
    if (ret == CR_SUCCESS)
    {
        *pdnDevInst = StringTableAddString(StringTable, pDeviceID, 1);
        if (*pdnDevInst == 0)
            ret = CR_NO_SUCH_DEVNODE;
    }

    return ret;
}


/***********************************************************************
 * CM_Delete_Class_Key [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Delete_Class_Key(
    LPGUID ClassGuid, ULONG ulFlags)
{
    TRACE("%p %lx\n", ClassGuid, ulFlags);
    return CM_Delete_Class_Key_Ex(ClassGuid, ulFlags, NULL);
}


/***********************************************************************
 * CM_Delete_Class_Key_Ex [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Delete_Class_Key_Ex(
    LPGUID ClassGuid, ULONG ulFlags, HANDLE hMachine)
{
    WCHAR szGuidString[MAX_GUID_STRING_LEN];
    RPC_BINDING_HANDLE BindingHandle = NULL;

    TRACE("%p %lx %lx\n", ClassGuid, ulFlags, hMachine);

    if (ClassGuid == NULL)
        return CR_INVALID_POINTER;

    if (ulFlags & ~CM_DELETE_CLASS_BITS)
        return CR_INVALID_FLAG;

    if (!GuidToString(ClassGuid, szGuidString))
        return CR_INVALID_DATA;

    if (hMachine != NULL)
    {
        BindingHandle = ((PMACHINE_INFO)hMachine)->BindingHandle;
        if (BindingHandle == NULL)
            return CR_FAILURE;
    }
    else
    {
        if (!PnpGetLocalHandles(&BindingHandle, NULL))
            return CR_FAILURE;
    }

    return PNP_DeleteClassKey(BindingHandle,
                             szGuidString,
                             ulFlags);
}


/***********************************************************************
 * CM_Disable_DevNode [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Disable_DevNode(
    DEVINST dnDevInst, ULONG ulFlags)
{
    TRACE("%p %lx\n", dnDevInst, ulFlags);
    return CM_Disable_DevNode_Ex(dnDevInst, ulFlags, NULL);
}


/***********************************************************************
 * CM_Disable_DevNode_Ex [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Disable_DevNode_Ex(
    DEVINST dnDevInst, ULONG ulFlags, HMACHINE hMachine)
{
    RPC_BINDING_HANDLE BindingHandle = NULL;
    HSTRING_TABLE StringTable = NULL;
    LPWSTR lpDevInst;

    FIXME("%p %lx %p\n", dnDevInst, ulFlags, hMachine);

    if (!IsUserAdmin())
        return CR_ACCESS_DENIED;

    if (dnDevInst == 0)
        return CR_INVALID_DEVINST;

    if (ulFlags != 0)
        return CR_INVALID_FLAG;

    if (hMachine != NULL)
    {
        BindingHandle = ((PMACHINE_INFO)hMachine)->BindingHandle;
        if (BindingHandle == NULL)
            return CR_FAILURE;

        StringTable = ((PMACHINE_INFO)hMachine)->StringTable;
        if (StringTable == 0)
            return CR_FAILURE;
    }
    else
    {
        if (!PnpGetLocalHandles(&BindingHandle, &StringTable))
            return CR_FAILURE;
    }

    lpDevInst = StringTableStringFromId(StringTable, dnDevInst);
    if (lpDevInst == NULL)
        return CR_INVALID_DEVNODE;

    return PNP_DeviceInstanceAction(BindingHandle,
                                    5,
                                    ulFlags,
                                    lpDevInst,
                                    NULL);
}


/***********************************************************************
 * CM_Disconnect_Machine [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Disconnect_Machine(HMACHINE hMachine)
{
    PMACHINE_INFO pMachine;

    TRACE("%lx\n", hMachine);

    pMachine = (PMACHINE_INFO)hMachine;
    if (pMachine == NULL)
        return CR_SUCCESS;

    if (pMachine->StringTable != NULL)
        StringTableDestroy(pMachine->StringTable);

    if (!PnpUnbindRpc(pMachine->BindingHandle))
        return CR_ACCESS_DENIED;

    HeapFree(GetProcessHeap(), 0, pMachine);

    return CR_SUCCESS;
}


/***********************************************************************
 * CM_Enable_DevNode [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Enable_DevNode(
    DEVINST dnDevInst, ULONG ulFlags)
{
    TRACE("%p %lx\n", dnDevInst, ulFlags);
    return CM_Enable_DevNode_Ex(dnDevInst, ulFlags, NULL);
}


/***********************************************************************
 * CM_Enable_DevNode_Ex [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Enable_DevNode_Ex(
    DEVINST dnDevInst, ULONG ulFlags, HMACHINE hMachine)
{
    RPC_BINDING_HANDLE BindingHandle = NULL;
    HSTRING_TABLE StringTable = NULL;
    LPWSTR lpDevInst;

    FIXME("%p %lx %p\n", dnDevInst, ulFlags, hMachine);

    if (!IsUserAdmin())
        return CR_ACCESS_DENIED;

    if (dnDevInst == 0)
        return CR_INVALID_DEVINST;

    if (ulFlags != 0)
        return CR_INVALID_FLAG;

    if (hMachine != NULL)
    {
        BindingHandle = ((PMACHINE_INFO)hMachine)->BindingHandle;
        if (BindingHandle == NULL)
            return CR_FAILURE;

        StringTable = ((PMACHINE_INFO)hMachine)->StringTable;
        if (StringTable == 0)
            return CR_FAILURE;
    }
    else
    {
        if (!PnpGetLocalHandles(&BindingHandle, &StringTable))
            return CR_FAILURE;
    }

    lpDevInst = StringTableStringFromId(StringTable, dnDevInst);
    if (lpDevInst == NULL)
        return CR_INVALID_DEVNODE;

    return PNP_DeviceInstanceAction(BindingHandle,
                                    4,
                                    ulFlags,
                                    lpDevInst,
                                    NULL);
}


/***********************************************************************
 * CM_Enumerate_Classes [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Enumerate_Classes(
    ULONG ulClassIndex, LPGUID ClassGuid, ULONG ulFlags)
{
    TRACE("%lx %p %lx\n", ulClassIndex, ClassGuid, ulFlags);
    return CM_Enumerate_Classes_Ex(ulClassIndex, ClassGuid, ulFlags, NULL);
}


/***********************************************************************
 * CM_Enumerate_Classes_Ex [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Enumerate_Classes_Ex(
    ULONG ulClassIndex, LPGUID ClassGuid, ULONG ulFlags, HMACHINE hMachine)
{
    WCHAR szBuffer[MAX_GUID_STRING_LEN];
    RPC_BINDING_HANDLE BindingHandle = NULL;
    CONFIGRET ret = CR_SUCCESS;
    ULONG ulLength = MAX_GUID_STRING_LEN;

    TRACE("%lx %p %lx %p\n", ulClassIndex, ClassGuid, ulFlags, hMachine);

    if (ClassGuid == NULL)
        return CR_INVALID_POINTER;

    if (ulFlags != 0)
        return CR_INVALID_FLAG;

    if (hMachine != NULL)
    {
        BindingHandle = ((PMACHINE_INFO)hMachine)->BindingHandle;
        if (BindingHandle == NULL)
            return CR_FAILURE;
    }
    else
    {
        if (!PnpGetLocalHandles(&BindingHandle, NULL))
            return CR_FAILURE;
    }

    ret = PNP_EnumerateSubKeys(BindingHandle,
                               PNP_BRANCH_CLASS,
                               ulClassIndex,
                               szBuffer,
                               MAX_GUID_STRING_LEN,
                               &ulLength,
                               ulFlags);
    if (ret == CR_SUCCESS)
    {
        /* Remove the {} */
        szBuffer[MAX_GUID_STRING_LEN - 2] = UNICODE_NULL;

        /* Convert the buffer to a GUID */
        if (UuidFromStringW(&szBuffer[1], ClassGuid) != RPC_S_OK)
            return CR_FAILURE;
    }

    return ret;
}


/***********************************************************************
 * CM_Enumerate_EnumeratorsA [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Enumerate_EnumeratorsA(
    ULONG ulEnumIndex, PCHAR Buffer, PULONG pulLength, ULONG ulFlags)
{
    TRACE("%lu %p %p %lx\n", ulEnumIndex, Buffer, pulLength, ulFlags);
    return CM_Enumerate_Enumerators_ExA(ulEnumIndex, Buffer, pulLength,
                                        ulFlags, NULL);
}


/***********************************************************************
 * CM_Enumerate_EnumeratorsW [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Enumerate_EnumeratorsW(
    ULONG ulEnumIndex, PWCHAR Buffer, PULONG pulLength, ULONG ulFlags)
{
    TRACE("%lu %p %p %lx\n", ulEnumIndex, Buffer, pulLength, ulFlags);
    return CM_Enumerate_Enumerators_ExW(ulEnumIndex, Buffer, pulLength,
                                        ulFlags, NULL);
}


/***********************************************************************
 * CM_Enumerate_Enumerators_ExA [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Enumerate_Enumerators_ExA(
    ULONG ulEnumIndex, PCHAR Buffer, PULONG pulLength, ULONG ulFlags,
    HMACHINE hMachine)
{
    WCHAR szBuffer[MAX_DEVICE_ID_LEN];
    ULONG ulOrigLength;
    ULONG ulLength;
    CONFIGRET ret = CR_SUCCESS;

    TRACE("%lu %p %p %lx %lx\n", ulEnumIndex, Buffer, pulLength, ulFlags,
          hMachine);

    if (Buffer == NULL || pulLength == NULL)
        return CR_INVALID_POINTER;

    if (ulFlags != 0)
        return CR_INVALID_FLAG;

    ulOrigLength = *pulLength;
    *pulLength = 0;

    ulLength = MAX_DEVICE_ID_LEN;
    ret = CM_Enumerate_Enumerators_ExW(ulEnumIndex, szBuffer, &ulLength,
                                       ulFlags, hMachine);
    if (ret == CR_SUCCESS)
    {
        if (WideCharToMultiByte(CP_ACP,
                                0,
                                szBuffer,
                                ulLength,
                                Buffer,
                                ulOrigLength,
                                NULL,
                                NULL) == 0)
            ret = CR_FAILURE;
        else
            *pulLength = lstrlenA(Buffer) + 1;
    }

    return ret;
}


/***********************************************************************
 * CM_Enumerate_Enumerators_ExW [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Enumerate_Enumerators_ExW(
    ULONG ulEnumIndex, PWCHAR Buffer, PULONG pulLength, ULONG ulFlags,
    HMACHINE hMachine)
{
    RPC_BINDING_HANDLE BindingHandle = NULL;

    TRACE("%lu %p %p %lx %lx\n", ulEnumIndex, Buffer, pulLength, ulFlags,
          hMachine);

    if (Buffer == NULL || pulLength == NULL)
        return CR_INVALID_POINTER;

    if (ulFlags != 0)
        return CR_INVALID_FLAG;

    *Buffer = UNICODE_NULL;

    if (hMachine != NULL)
    {
        BindingHandle = ((PMACHINE_INFO)hMachine)->BindingHandle;
        if (BindingHandle == NULL)
            return CR_FAILURE;
    }
    else
    {
        if (!PnpGetLocalHandles(&BindingHandle, NULL))
            return CR_FAILURE;
    }

    return PNP_EnumerateSubKeys(BindingHandle,
                                PNP_BRANCH_ENUM,
                                ulEnumIndex,
                                Buffer,
                                *pulLength,
                                pulLength,
                                ulFlags);
}


/***********************************************************************
 * CM_Get_Child [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Get_Child(
    PDEVINST pdnDevInst, DEVINST dnDevInst, ULONG ulFlags)
{
    TRACE("%p %p %lx\n", pdnDevInst, dnDevInst, ulFlags);
    return CM_Get_Child_Ex(pdnDevInst, dnDevInst, ulFlags, NULL);
}


/***********************************************************************
 * CM_Get_Child_Ex [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Get_Child_Ex(
    PDEVINST pdnDevInst, DEVINST dnDevInst, ULONG ulFlags, HMACHINE hMachine)
{
    WCHAR szRelatedDevInst[MAX_DEVICE_ID_LEN];
    RPC_BINDING_HANDLE BindingHandle = NULL;
    HSTRING_TABLE StringTable = NULL;
    LPWSTR lpDevInst;
    DWORD dwIndex;
    CONFIGRET ret;

    TRACE("%p %lx %lx %lx\n", pdnDevInst, dnDevInst, ulFlags, hMachine);

    if (pdnDevInst == NULL)
        return CR_INVALID_POINTER;

    if (dnDevInst == 0)
        return CR_INVALID_DEVINST;

    if (ulFlags != 0)
        return CR_INVALID_FLAG;

    *pdnDevInst = -1;

    if (hMachine != NULL)
    {
        BindingHandle = ((PMACHINE_INFO)hMachine)->BindingHandle;
        if (BindingHandle == NULL)
            return CR_FAILURE;

        StringTable = ((PMACHINE_INFO)hMachine)->StringTable;
        if (StringTable == 0)
            return CR_FAILURE;
    }
    else
    {
        if (!PnpGetLocalHandles(&BindingHandle, &StringTable))
            return CR_FAILURE;
    }

    lpDevInst = StringTableStringFromId(StringTable, dnDevInst);
    if (lpDevInst == NULL)
        return CR_INVALID_DEVNODE;

    ret = PNP_GetRelatedDeviceInstance(BindingHandle,
                                       PNP_DEVICE_CHILD,
                                       lpDevInst,
                                       szRelatedDevInst,
                                       MAX_DEVICE_ID_LEN,
                                       0);
    if (ret != CR_SUCCESS)
        return ret;

    TRACE("szRelatedDevInst: %s\n", debugstr_w(szRelatedDevInst));

    dwIndex = StringTableAddString(StringTable, szRelatedDevInst, 1);
    if (dwIndex == -1)
        return CR_FAILURE;

    *pdnDevInst = dwIndex;

    return CR_SUCCESS;
}


/***********************************************************************
 * CM_Get_Class_Key_NameA [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Get_Class_Key_NameA(
    LPGUID ClassGuid, LPSTR pszKeyName, PULONG pulLength, ULONG ulFlags)
{
    TRACE("%p %p %p %lx\n",
          ClassGuid, pszKeyName, pulLength, ulFlags);
    return CM_Get_Class_Key_Name_ExA(ClassGuid, pszKeyName, pulLength,
                                     ulFlags, NULL);
}


/***********************************************************************
 * CM_Get_Class_Key_NameW [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Get_Class_Key_NameW(
    LPGUID ClassGuid, LPWSTR pszKeyName, PULONG pulLength, ULONG ulFlags)
{
    TRACE("%p %p %p %lx\n",
          ClassGuid, pszKeyName, pulLength, ulFlags);
    return CM_Get_Class_Key_Name_ExW(ClassGuid, pszKeyName, pulLength,
                                     ulFlags, NULL);
}


/***********************************************************************
 * CM_Get_Class_Key_Name_ExA [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Get_Class_Key_Name_ExA(
    LPGUID ClassGuid, LPSTR pszKeyName, PULONG pulLength, ULONG ulFlags,
    HMACHINE hMachine)
{
    WCHAR szBuffer[MAX_GUID_STRING_LEN];
    CONFIGRET ret = CR_SUCCESS;
    ULONG ulLength;
    ULONG ulOrigLength;

    TRACE("%p %p %p %lx %lx\n",
          ClassGuid, pszKeyName, pulLength, ulFlags, hMachine);

    if (ClassGuid == NULL || pszKeyName == NULL || pulLength == NULL)
        return CR_INVALID_POINTER;

    ulOrigLength = *pulLength;
    *pulLength = 0;

    ulLength = MAX_GUID_STRING_LEN;
    ret = CM_Get_Class_Key_Name_ExW(ClassGuid, szBuffer, &ulLength,
                                    ulFlags, hMachine);
    if (ret == CR_SUCCESS)
    {
        if (WideCharToMultiByte(CP_ACP,
                                0,
                                szBuffer,
                                ulLength,
                                pszKeyName,
                                ulOrigLength,
                                NULL,
                                NULL) == 0)
            ret = CR_FAILURE;
        else
            *pulLength = lstrlenA(pszKeyName) + 1;
    }

    return CR_SUCCESS;
}


/***********************************************************************
 * CM_Get_Class_Key_Name_ExW [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Get_Class_Key_Name_ExW(
    LPGUID ClassGuid, LPWSTR pszKeyName, PULONG pulLength, ULONG ulFlags,
    HMACHINE hMachine)
{
    TRACE("%p %p %p %lx %lx\n",
          ClassGuid, pszKeyName, pulLength, ulFlags, hMachine);

    if (ClassGuid == NULL || pszKeyName == NULL || pulLength == NULL)
        return CR_INVALID_POINTER;

    if (ulFlags != 0)
        return CR_INVALID_FLAG;

    if (*pulLength < MAX_GUID_STRING_LEN)
    {
        *pulLength = 0;
        return CR_BUFFER_SMALL;
    }

    if (!GuidToString(ClassGuid, pszKeyName))
        return CR_INVALID_DATA;

    *pulLength = MAX_GUID_STRING_LEN;

    return CR_SUCCESS;
}


/***********************************************************************
 * CM_Get_Class_NameA [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Get_Class_NameA(
    LPGUID ClassGuid, PCHAR Buffer, PULONG pulLength, ULONG ulFlags)
{
    TRACE("%p %p %p %lx\n", ClassGuid, Buffer, pulLength, ulFlags);
    return CM_Get_Class_Name_ExA(ClassGuid, Buffer, pulLength, ulFlags,
                                 NULL);
}


/***********************************************************************
 * CM_Get_Class_NameW [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Get_Class_NameW(
    LPGUID ClassGuid, PWCHAR Buffer, PULONG pulLength, ULONG ulFlags)
{
    TRACE("%p %p %p %lx\n", ClassGuid, Buffer, pulLength, ulFlags);
    return CM_Get_Class_Name_ExW(ClassGuid, Buffer, pulLength, ulFlags,
                                 NULL);
}


/***********************************************************************
 * CM_Get_Class_Name_ExA [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Get_Class_Name_ExA(
    LPGUID ClassGuid, PCHAR Buffer, PULONG pulLength, ULONG ulFlags,
    HMACHINE hMachine)
{
    WCHAR szBuffer[MAX_CLASS_NAME_LEN];
    CONFIGRET ret = CR_SUCCESS;
    ULONG ulLength;
    ULONG ulOrigLength;

    TRACE("%p %p %p %lx %lx\n",
          ClassGuid, Buffer, pulLength, ulFlags, hMachine);

    if (ClassGuid == NULL || Buffer == NULL || pulLength == NULL)
        return CR_INVALID_POINTER;

    ulOrigLength = *pulLength;
    *pulLength = 0;

    ulLength = MAX_CLASS_NAME_LEN;
    ret = CM_Get_Class_Name_ExW(ClassGuid, szBuffer, &ulLength,
                                ulFlags, hMachine);
    if (ret == CR_SUCCESS)
    {
        if (WideCharToMultiByte(CP_ACP,
                                0,
                                szBuffer,
                                ulLength,
                                Buffer,
                                ulOrigLength,
                                NULL,
                                NULL) == 0)
            ret = CR_FAILURE;
        else
            *pulLength = lstrlenA(Buffer) + 1;
    }

    return ret;
}


/***********************************************************************
 * CM_Get_Class_Name_ExW [SETUPAPI.@]
 */
CONFIGRET WINAPI
CM_Get_Class_Name_ExW(
    LPGUID ClassGuid, PWCHAR Buffer, PULONG pulLength, ULONG ulFlags,
    HMACHINE hMachine)
{
    WCHAR szGuidString[MAX_GUID_STRING_LEN];
    RPC_BINDING_HANDLE BindingHandle = NULL;

    TRACE("%p %p %p %lx %lx\n",
          ClassGuid, Buffer, pulLength, ulFlags, hMachine);

    if (ClassGuid == NULL || Buffer == NULL || pulLength == NULL)
        return CR_INVALID_POINTER;

    if (ulFlags != 0)
        return CR_INVALID_FLAG;

    if (!GuidToString(ClassGuid, szGuidString))
        return CR_INVALID_DATA;

    TRACE("Guid %s\n", debugstr_w(szGuidString));

    if (hMachine != NULL)
    {
        BindingHandle = ((PMACHINE_INFO)hMachine)->BindingHandle;
        if (BindingHandle == NULL)
            return CR_FAILURE;
    }
    else
    {
        if (!PnpGetLocalHandles(&BindingHandle, NULL))
            return CR_FAILURE;
    }

    return PNP_GetClassName(BindingHandle,
                            szGuidString,
                            Buffer,
                            pulLength,
                            ulFlags);
}


/***********************************************************************
 * CM_Get_Depth [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Get_Depth(
    PULONG pulDepth, DEVINST dnDevInst, ULONG ulFlags)
{
    TRACE("%p %lx %lx\n", pulDepth, dnDevInst, ulFlags);
    return CM_Get_Depth_Ex(pulDepth, dnDevInst, ulFlags, NULL);
}


/***********************************************************************
 * CM_Get_Depth_Ex [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Get_Depth_Ex(
    PULONG pulDepth, DEVINST dnDevInst, ULONG ulFlags, HMACHINE hMachine)
{
    RPC_BINDING_HANDLE BindingHandle = NULL;
    HSTRING_TABLE StringTable = NULL;
    LPWSTR lpDevInst;

    TRACE("%p %lx %lx %lx\n",
          pulDepth, dnDevInst, ulFlags, hMachine);

    if (pulDepth == NULL)
        return CR_INVALID_POINTER;

    if (dnDevInst == 0)
        return CR_INVALID_DEVINST;

    if (ulFlags != 0)
        return CR_INVALID_FLAG;

    if (hMachine != NULL)
    {
        BindingHandle = ((PMACHINE_INFO)hMachine)->BindingHandle;
        if (BindingHandle == NULL)
            return CR_FAILURE;

        StringTable = ((PMACHINE_INFO)hMachine)->StringTable;
        if (StringTable == 0)
            return CR_FAILURE;
    }
    else
    {
        if (!PnpGetLocalHandles(&BindingHandle, &StringTable))
            return CR_FAILURE;
    }

    lpDevInst = StringTableStringFromId(StringTable, dnDevInst);
    if (lpDevInst == NULL)
        return CR_INVALID_DEVNODE;

    return PNP_GetDepth(BindingHandle,
                        lpDevInst,
                        pulDepth,
                        ulFlags);
}


/***********************************************************************
 * CM_Get_DevNode_Registry_PropertyA [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Get_DevNode_Registry_PropertyA(
    DEVINST dnDevInst, ULONG ulProperty, PULONG pulRegDataType,
    PVOID Buffer, PULONG pulLength, ULONG ulFlags)
{
    TRACE("%lx %lu %p %p %p %lx\n",
          dnDevInst, ulProperty, pulRegDataType, Buffer, pulLength, ulFlags);

    return CM_Get_DevNode_Registry_Property_ExA(dnDevInst, ulProperty,
                                                pulRegDataType, Buffer,
                                                pulLength, ulFlags, NULL);
}


/***********************************************************************
 * CM_Get_DevNode_Registry_PropertyW [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Get_DevNode_Registry_PropertyW(
    DEVINST dnDevInst, ULONG ulProperty, PULONG pulRegDataType,
    PVOID Buffer, PULONG pulLength, ULONG ulFlags)
{
    TRACE("%lx %lu %p %p %p %lx\n",
          dnDevInst, ulProperty, pulRegDataType, Buffer, pulLength, ulFlags);

    return CM_Get_DevNode_Registry_Property_ExW(dnDevInst, ulProperty,
                                                pulRegDataType, Buffer,
                                                pulLength, ulFlags, NULL);
}


/***********************************************************************
 * CM_Get_DevNode_Registry_Property_ExA [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Get_DevNode_Registry_Property_ExA(
    DEVINST dnDevInst, ULONG ulProperty, PULONG pulRegDataType,
    PVOID Buffer, PULONG pulLength, ULONG ulFlags, HMACHINE hMachine)
{
    PVOID BufferW;
    ULONG LengthW;
    ULONG RegDataType;
    CONFIGRET ret;

    TRACE("%lx %lu %p %p %p %lx %lx\n",
          dnDevInst, ulProperty, pulRegDataType, Buffer, pulLength,
          ulFlags, hMachine);

    if (!pulLength)
        return CR_INVALID_POINTER;

    LengthW = *pulLength * sizeof(WCHAR);
    BufferW = HeapAlloc(GetProcessHeap(), 0, LengthW);

    if (!BufferW)
        return CR_OUT_OF_MEMORY;

    ret = CM_Get_DevNode_Registry_Property_ExW(dnDevInst,
                                               ulProperty,
                                               &RegDataType,
                                               BufferW,
                                               &LengthW,
                                               ulFlags,
                                               hMachine);

    if (ret == CR_SUCCESS)
    {
        if (RegDataType == REG_SZ || RegDataType == REG_EXPAND_SZ)
        {
            /* Do W->A conversion */
            *pulLength = WideCharToMultiByte(CP_ACP,
                                             0,
                                             BufferW,
                                             lstrlenW(BufferW) + 1,
                                             Buffer,
                                             *pulLength,
                                             NULL,
                                             NULL);
            if (*pulLength == 0)
                ret = CR_FAILURE;
        }
        else
        {
            /* Directly copy the value */
            if (LengthW <= *pulLength)
                memcpy(Buffer, BufferW, LengthW);
            else
            {
                *pulLength = LengthW;
                ret = CR_BUFFER_SMALL;
            }
        }
    }

    if (pulRegDataType)
        *pulRegDataType = RegDataType;

    HeapFree(GetProcessHeap(), 0, BufferW);

    return ret;
}


/***********************************************************************
 * CM_Get_DevNode_Registry_Property_ExW [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Get_DevNode_Registry_Property_ExW(
    DEVINST dnDevInst, ULONG ulProperty, PULONG pulRegDataType,
    PVOID Buffer, PULONG pulLength, ULONG ulFlags, HMACHINE hMachine)
{
    RPC_BINDING_HANDLE BindingHandle = NULL;
    HSTRING_TABLE StringTable = NULL;
    CONFIGRET ret = CR_SUCCESS;
    LPWSTR lpDevInst;
    ULONG ulDataType = 0;
    ULONG ulTransferLength = 0;

    TRACE("%lx %lu %p %p %p %lx %lx\n",
          dnDevInst, ulProperty, pulRegDataType, Buffer, pulLength,
          ulFlags, hMachine);

    if (dnDevInst == 0)
        return CR_INVALID_DEVNODE;

    if (ulProperty < CM_DRP_MIN || ulProperty > CM_DRP_MAX)
        return CR_INVALID_PROPERTY;

    /* pulRegDataType is optional */

    /* Buffer is optional */

    if (pulLength == NULL)
        return CR_INVALID_POINTER;

    if (*pulLength == 0)
        return CR_INVALID_POINTER;

    if (ulFlags != 0)
        return CR_INVALID_FLAG;

    if (hMachine != NULL)
    {
        BindingHandle = ((PMACHINE_INFO)hMachine)->BindingHandle;
        if (BindingHandle == NULL)
            return CR_FAILURE;

        StringTable = ((PMACHINE_INFO)hMachine)->StringTable;
        if (StringTable == 0)
            return CR_FAILURE;
    }
    else
    {
        if (!PnpGetLocalHandles(&BindingHandle, &StringTable))
            return CR_FAILURE;
    }

    lpDevInst = StringTableStringFromId(StringTable, dnDevInst);
    if (lpDevInst == NULL)
        return CR_INVALID_DEVNODE;

    ulTransferLength = *pulLength;
    ret = PNP_GetDeviceRegProp(BindingHandle,
                               lpDevInst,
                               ulProperty,
                               &ulDataType,
                               Buffer,
                               &ulTransferLength,
                               pulLength,
                               ulFlags);
    if (ret == CR_SUCCESS)
    {
        if (pulRegDataType != NULL)
            *pulRegDataType = ulDataType;
    }

    return ret;
}


/***********************************************************************
 * CM_Get_DevNode_Status [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Get_DevNode_Status(
    PULONG pulStatus, PULONG pulProblemNumber, DEVINST dnDevInst,
    ULONG ulFlags)
{
    TRACE("%p %p %lx %lx\n",
          pulStatus, pulProblemNumber, dnDevInst, ulFlags);
    return CM_Get_DevNode_Status_Ex(pulStatus, pulProblemNumber, dnDevInst,
                                    ulFlags, NULL);
}


/***********************************************************************
 * CM_Get_DevNode_Status_Ex [SETUPAPI.@]
 */
CONFIGRET WINAPI
CM_Get_DevNode_Status_Ex(
    PULONG pulStatus, PULONG pulProblemNumber, DEVINST dnDevInst,
    ULONG ulFlags, HMACHINE hMachine)
{
    RPC_BINDING_HANDLE BindingHandle = NULL;
    HSTRING_TABLE StringTable = NULL;
    LPWSTR lpDevInst;

    TRACE("%p %p %lx %lx %lx\n",
          pulStatus, pulProblemNumber, dnDevInst, ulFlags, hMachine);

    if (pulStatus == NULL || pulProblemNumber == NULL)
        return CR_INVALID_POINTER;

    if (dnDevInst == 0)
        return CR_INVALID_DEVINST;

    if (ulFlags != 0)
        return CR_INVALID_FLAG;

    if (hMachine != NULL)
    {
        BindingHandle = ((PMACHINE_INFO)hMachine)->BindingHandle;
        if (BindingHandle == NULL)
            return CR_FAILURE;

        StringTable = ((PMACHINE_INFO)hMachine)->StringTable;
        if (StringTable == 0)
            return CR_FAILURE;
    }
    else
    {
        if (!PnpGetLocalHandles(&BindingHandle, &StringTable))
            return CR_FAILURE;
    }

    lpDevInst = StringTableStringFromId(StringTable, dnDevInst);
    if (lpDevInst == NULL)
        return CR_INVALID_DEVNODE;

    return PNP_GetDeviceStatus(BindingHandle,
                               lpDevInst,
                               pulStatus,
                               pulProblemNumber,
                               ulFlags);
}


/***********************************************************************
 * CM_Get_Device_IDA [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Get_Device_IDA(
    DEVINST dnDevInst, PCHAR Buffer, ULONG BufferLen, ULONG ulFlags)
{
    TRACE("%lx %p %ld %ld\n",
          dnDevInst, Buffer, BufferLen, ulFlags);
    return CM_Get_Device_ID_ExA(dnDevInst, Buffer, BufferLen, ulFlags, NULL);
}


/***********************************************************************
 * CM_Get_Device_IDW [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Get_Device_IDW(
    DEVINST dnDevInst, PWCHAR Buffer, ULONG BufferLen, ULONG ulFlags)
{
    TRACE("%lx %p %ld %ld\n",
          dnDevInst, Buffer, BufferLen, ulFlags);
    return CM_Get_Device_ID_ExW(dnDevInst, Buffer, BufferLen, ulFlags, NULL);
}


/***********************************************************************
 * CM_Get_Device_ID_ExA [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Get_Device_ID_ExA(
    DEVINST dnDevInst, PCHAR Buffer, ULONG BufferLen, ULONG ulFlags,
    HMACHINE hMachine)
{
    WCHAR szBufferW[MAX_DEVICE_ID_LEN];
    CONFIGRET ret = CR_SUCCESS;

    FIXME("%lx %p %ld %ld %lx\n",
          dnDevInst, Buffer, BufferLen, ulFlags, hMachine);

    if (Buffer == NULL)
        return CR_INVALID_POINTER;

    ret = CM_Get_Device_ID_ExW(dnDevInst,
                               szBufferW,
                               MAX_DEVICE_ID_LEN,
                               ulFlags,
                               hMachine);
    if (ret == CR_SUCCESS)
    {
        if (WideCharToMultiByte(CP_ACP,
                                0,
                                szBufferW,
                                lstrlenW(szBufferW) + 1,
                                Buffer,
                                BufferLen,
                                NULL,
                                NULL) == 0)
            ret = CR_FAILURE;
    }

    return ret;
}


/***********************************************************************
 * CM_Get_Device_ID_ExW [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Get_Device_ID_ExW(
    DEVINST dnDevInst, PWCHAR Buffer, ULONG BufferLen, ULONG ulFlags,
    HMACHINE hMachine)
{
    HSTRING_TABLE StringTable = NULL;

    TRACE("%lx %p %ld %ld %lx\n",
          dnDevInst, Buffer, BufferLen, ulFlags, hMachine);

    if (dnDevInst == 0)
        return CR_INVALID_DEVINST;

    if (Buffer == NULL)
        return CR_INVALID_POINTER;

    if (ulFlags != 0)
        return CR_INVALID_FLAG;

    if (hMachine != NULL)
    {
        StringTable = ((PMACHINE_INFO)hMachine)->StringTable;
        if (StringTable == NULL)
            return CR_FAILURE;
    }
    else
    {
        if (!PnpGetLocalHandles(NULL, &StringTable))
            return CR_FAILURE;
    }

    if (!StringTableStringFromIdEx(StringTable,
                                   dnDevInst,
                                   Buffer,
                                   &BufferLen))
        return CR_FAILURE;

    return CR_SUCCESS;
}


/***********************************************************************
 * CM_Get_Device_ID_ListA [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Get_Device_ID_ListA(
    PCSTR pszFilter, PCHAR Buffer, ULONG BufferLen, ULONG ulFlags)
{
    TRACE("%p %p %ld %ld\n", pszFilter, Buffer, BufferLen, ulFlags);
    return CM_Get_Device_ID_List_ExA(pszFilter, Buffer, BufferLen,
                                     ulFlags, NULL);
}


/***********************************************************************
 * CM_Get_Device_ID_ListW [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Get_Device_ID_ListW(
    PCWSTR pszFilter, PWCHAR Buffer, ULONG BufferLen, ULONG ulFlags)
{
    TRACE("%p %p %ld %ld\n", pszFilter, Buffer, BufferLen, ulFlags);
    return CM_Get_Device_ID_List_ExW(pszFilter, Buffer, BufferLen,
                                     ulFlags, NULL);
}


/***********************************************************************
 * CM_Get_Device_ID_List_ExA [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Get_Device_ID_List_ExA(
    PCSTR pszFilter, PCHAR Buffer, ULONG BufferLen, ULONG ulFlags,
    HMACHINE hMachine)
{
    LPWSTR BufferW = NULL;
    LPWSTR pszFilterW = NULL;
    CONFIGRET ret = CR_SUCCESS;

    FIXME("%p %p %ld %ld %lx\n",
          pszFilter, Buffer, BufferLen, ulFlags, hMachine);

    BufferW = MyMalloc(BufferLen * sizeof(WCHAR));
    if (BufferW == NULL)
        return CR_OUT_OF_MEMORY;

    if (pszFilter == NULL)
    {
        ret = CM_Get_Device_ID_List_ExW(NULL,
                                        BufferW,
                                        BufferLen,
                                        ulFlags,
                                        hMachine);
    }
    else
    {
        if (CaptureAndConvertAnsiArg(pszFilter, &pszFilterW))
        {
            ret = CR_INVALID_DEVICE_ID;
            goto Done;
        }

        ret = CM_Get_Device_ID_List_ExW(pszFilterW,
                                        BufferW,
                                        BufferLen,
                                        ulFlags,
                                        hMachine);

        MyFree(pszFilterW);
    }

    if (WideCharToMultiByte(CP_ACP,
                            0,
                            BufferW,
                            lstrlenW(BufferW) + 1,
                            Buffer,
                            BufferLen,
                            NULL,
                            NULL) == 0)
        ret = CR_FAILURE;

Done:
    MyFree(BufferW);

    return ret;
}


/***********************************************************************
 * CM_Get_Device_ID_List_ExW [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Get_Device_ID_List_ExW(
    PCWSTR pszFilter, PWCHAR Buffer, ULONG BufferLen, ULONG ulFlags,
    HMACHINE hMachine)
{
    FIXME("%p %p %ld %ld %lx\n",
          pszFilter, Buffer, BufferLen, ulFlags, hMachine);
    memset(Buffer,0,2);
    return CR_SUCCESS;
}


/***********************************************************************
 * CM_Get_Device_ID_List_SizeA [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Get_Device_ID_List_SizeA(
    PULONG pulLen, PCSTR pszFilter, ULONG ulFlags)
{
    TRACE("%p %s %ld\n", pulLen, pszFilter, ulFlags);
    return CM_Get_Device_ID_List_Size_ExA(pulLen, pszFilter, ulFlags, NULL);
}


/***********************************************************************
 * CM_Get_Device_ID_List_SizeW [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Get_Device_ID_List_SizeW(
    PULONG pulLen, PCWSTR pszFilter, ULONG ulFlags)
{
    TRACE("%p %s %ld\n", pulLen, debugstr_w(pszFilter), ulFlags);
    return CM_Get_Device_ID_List_Size_ExW(pulLen, pszFilter, ulFlags, NULL);
}


/***********************************************************************
 * CM_Get_Device_ID_List_Size_ExA [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Get_Device_ID_List_Size_ExA(
    PULONG pulLen, PCSTR pszFilter, ULONG ulFlags, HMACHINE hMachine)
{
    LPWSTR pszFilterW = NULL;
    CONFIGRET ret = CR_SUCCESS;

    FIXME("%p %s %lx %lx\n", pulLen, pszFilter, ulFlags, hMachine);

    if (pszFilter == NULL)
    {
        ret = CM_Get_Device_ID_List_Size_ExW(pulLen,
                                             NULL,
                                             ulFlags,
                                             hMachine);
    }
    else
    {
        if (CaptureAndConvertAnsiArg(pszFilter, &pszFilterW))
            return CR_INVALID_DEVICE_ID;

        ret = CM_Get_Device_ID_List_Size_ExW(pulLen,
                                             pszFilterW,
                                             ulFlags,
                                             hMachine);

        MyFree(pszFilterW);
    }

    return ret;
}


/***********************************************************************
 * CM_Get_Device_ID_List_Size_ExW [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Get_Device_ID_List_Size_ExW(
    PULONG pulLen, PCWSTR pszFilter, ULONG ulFlags, HMACHINE hMachine)
{
    RPC_BINDING_HANDLE BindingHandle = NULL;

    FIXME("%p %s %ld %lx\n", pulLen, debugstr_w(pszFilter), ulFlags, hMachine);

    if (pulLen == NULL)
        return CR_INVALID_POINTER;

    if (ulFlags & ~CM_GETIDLIST_FILTER_BITS)
        return CR_INVALID_FLAG;

    if (hMachine != NULL)
    {
        BindingHandle = ((PMACHINE_INFO)hMachine)->BindingHandle;
        if (BindingHandle == NULL)
            return CR_FAILURE;
    }
    else
    {
        if (!PnpGetLocalHandles(&BindingHandle, NULL))
            return CR_FAILURE;
    }

    *pulLen = 0;

    return PNP_GetDeviceListSize(BindingHandle,
                                 (LPWSTR)pszFilter,
                                 pulLen,
                                 ulFlags);
}


/***********************************************************************
 * CM_Get_Device_ID_Size [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Get_Device_ID_Size(
    PULONG pulLen, DEVINST dnDevInst, ULONG ulFlags)
{
    TRACE("%p %lx %lx\n", pulLen, dnDevInst, ulFlags);
    return CM_Get_Device_ID_Size_Ex(pulLen, dnDevInst, ulFlags, NULL);
}


/***********************************************************************
 * CM_Get_Device_ID_Size_Ex [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Get_Device_ID_Size_Ex(
    PULONG pulLen, DEVINST dnDevInst, ULONG ulFlags, HMACHINE hMachine)
{
    HSTRING_TABLE StringTable = NULL;
    LPWSTR DeviceId;

    TRACE("%p %lx %lx %lx\n", pulLen, dnDevInst, ulFlags, hMachine);

    if (pulLen == NULL)
        return CR_INVALID_POINTER;

    if (dnDevInst == 0)
        return CR_INVALID_DEVINST;

    if (ulFlags != 0)
        return CR_INVALID_FLAG;

    if (hMachine != NULL)
    {
        StringTable = ((PMACHINE_INFO)hMachine)->StringTable;
        if (StringTable == NULL)
            return CR_FAILURE;
    }
    else
    {
        if (!PnpGetLocalHandles(NULL, &StringTable))
            return CR_FAILURE;
    }

    DeviceId = StringTableStringFromId(StringTable, dnDevInst);
    if (DeviceId == NULL)
    {
        *pulLen = 0;
        return CR_SUCCESS;
    }

    *pulLen = lstrlenW(DeviceId);

    return CR_SUCCESS;
}


/***********************************************************************
 * CM_Get_Global_State [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Get_Global_State(
    PULONG pulState, ULONG ulFlags)
{
    TRACE("%p %lx\n", pulState, ulFlags);
    return CM_Get_Global_State_Ex(pulState, ulFlags, NULL);
}


/***********************************************************************
 * CM_Get_Global_State_Ex [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Get_Global_State_Ex(
    PULONG pulState, ULONG ulFlags, HMACHINE hMachine)
{
    RPC_BINDING_HANDLE BindingHandle = NULL;

    TRACE("%p %lx %lx\n", pulState, ulFlags, hMachine);

    if (pulState == NULL)
        return CR_INVALID_POINTER;

    if (ulFlags != 0)
        return CR_INVALID_FLAG;

    if (hMachine != NULL)
    {
        BindingHandle = ((PMACHINE_INFO)hMachine)->BindingHandle;
        if (BindingHandle == NULL)
            return CR_FAILURE;
    }
    else
    {
        if (!PnpGetLocalHandles(&BindingHandle, NULL))
            return CR_FAILURE;
    }

    return PNP_GetGlobalState(BindingHandle, pulState, ulFlags);
}


/***********************************************************************
 * CM_Get_Parent [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Get_Parent(
    PDEVINST pdnDevInst, DEVINST dnDevInst, ULONG ulFlags)
{
    TRACE("%p %p %lx\n", pdnDevInst, dnDevInst, ulFlags);
    return CM_Get_Parent_Ex(pdnDevInst, dnDevInst, ulFlags, NULL);
}


/***********************************************************************
 * CM_Get_Parent_Ex [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Get_Parent_Ex(
    PDEVINST pdnDevInst, DEVINST dnDevInst, ULONG ulFlags, HMACHINE hMachine)
{
    WCHAR szRelatedDevInst[MAX_DEVICE_ID_LEN];
    RPC_BINDING_HANDLE BindingHandle = NULL;
    HSTRING_TABLE StringTable = NULL;
    LPWSTR lpDevInst;
    DWORD dwIndex;
    CONFIGRET ret;

    TRACE("%p %lx %lx %lx\n", pdnDevInst, dnDevInst, ulFlags, hMachine);

    if (pdnDevInst == NULL)
        return CR_INVALID_POINTER;

    if (dnDevInst == 0)
        return CR_INVALID_DEVINST;

    if (ulFlags != 0)
        return CR_INVALID_FLAG;

    *pdnDevInst = -1;

    if (hMachine != NULL)
    {
        BindingHandle = ((PMACHINE_INFO)hMachine)->BindingHandle;
        if (BindingHandle == NULL)
            return CR_FAILURE;

        StringTable = ((PMACHINE_INFO)hMachine)->StringTable;
        if (StringTable == 0)
            return CR_FAILURE;
    }
    else
    {
        if (!PnpGetLocalHandles(&BindingHandle, &StringTable))
            return CR_FAILURE;
    }

    lpDevInst = StringTableStringFromId(StringTable, dnDevInst);
    if (lpDevInst == NULL)
        return CR_INVALID_DEVNODE;

    ret = PNP_GetRelatedDeviceInstance(BindingHandle,
                                       PNP_DEVICE_PARENT,
                                       lpDevInst,
                                       szRelatedDevInst,
                                       MAX_DEVICE_ID_LEN,
                                       0);
    if (ret != CR_SUCCESS)
        return ret;

    TRACE("szRelatedDevInst: %s\n", debugstr_w(szRelatedDevInst));

    dwIndex = StringTableAddString(StringTable, szRelatedDevInst, 1);
    if (dwIndex == -1)
        return CR_FAILURE;

    *pdnDevInst = dwIndex;

    return CR_SUCCESS;
}


/***********************************************************************
 * CM_Get_Sibling [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Get_Sibling(
    PDEVINST pdnDevInst, DEVINST dnDevInst, ULONG ulFlags)
{
    TRACE("%p %p %lx\n", pdnDevInst, dnDevInst, ulFlags);
    return CM_Get_Sibling_Ex(pdnDevInst, dnDevInst, ulFlags, NULL);
}


/***********************************************************************
 * CM_Get_Sibling_Ex [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Get_Sibling_Ex(
    PDEVINST pdnDevInst, DEVINST dnDevInst, ULONG ulFlags, HMACHINE hMachine)
{
    WCHAR szRelatedDevInst[MAX_DEVICE_ID_LEN];
    RPC_BINDING_HANDLE BindingHandle = NULL;
    HSTRING_TABLE StringTable = NULL;
    LPWSTR lpDevInst;
    DWORD dwIndex;
    CONFIGRET ret;

    TRACE("%p %lx %lx %lx\n", pdnDevInst, dnDevInst, ulFlags, hMachine);

    if (pdnDevInst == NULL)
        return CR_INVALID_POINTER;

    if (dnDevInst == 0)
        return CR_INVALID_DEVINST;

    if (ulFlags != 0)
        return CR_INVALID_FLAG;

    *pdnDevInst = -1;

    if (hMachine != NULL)
    {
        BindingHandle = ((PMACHINE_INFO)hMachine)->BindingHandle;
        if (BindingHandle == NULL)
            return CR_FAILURE;

        StringTable = ((PMACHINE_INFO)hMachine)->StringTable;
        if (StringTable == 0)
            return CR_FAILURE;
    }
    else
    {
        if (!PnpGetLocalHandles(&BindingHandle, &StringTable))
            return CR_FAILURE;
    }

    lpDevInst = StringTableStringFromId(StringTable, dnDevInst);
    if (lpDevInst == NULL)
        return CR_INVALID_DEVNODE;

    ret = PNP_GetRelatedDeviceInstance(BindingHandle,
                                       PNP_DEVICE_SIBLING,
                                       lpDevInst,
                                       szRelatedDevInst,
                                       MAX_DEVICE_ID_LEN,
                                       0);
    if (ret != CR_SUCCESS)
        return ret;

    TRACE("szRelatedDevInst: %s\n", debugstr_w(szRelatedDevInst));

    dwIndex = StringTableAddString(StringTable, szRelatedDevInst, 1);
    if (dwIndex == -1)
        return CR_FAILURE;

    *pdnDevInst = dwIndex;

    return CR_SUCCESS;
}


/***********************************************************************
 * CM_Get_Version [SETUPAPI.@]
 */
WORD WINAPI CM_Get_Version(VOID)
{
    TRACE("\n");
    return CM_Get_Version_Ex(NULL);
}


/***********************************************************************
 * CM_Get_Version_Ex [SETUPAPI.@]
 */
WORD WINAPI CM_Get_Version_Ex(HMACHINE hMachine)
{
    RPC_BINDING_HANDLE BindingHandle = NULL;
    WORD Version = 0;

    TRACE("%lx\n", hMachine);

    if (hMachine != NULL)
    {
        BindingHandle = ((PMACHINE_INFO)hMachine)->BindingHandle;
        if (BindingHandle == NULL)
            return 0;
    }
    else
    {
        if (!PnpGetLocalHandles(&BindingHandle, NULL))
            return CR_FAILURE;
    }

    if (PNP_GetVersion(BindingHandle, &Version) != CR_SUCCESS)
        return 0;

    return Version;
}


/***********************************************************************
 * CM_Is_Dock_Station_Present [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Is_Dock_Station_Present(
    PBOOL pbPresent)
{
    TRACE("%p\n", pbPresent);
    return CM_Is_Dock_Station_Present_Ex(pbPresent, NULL);
}


/***********************************************************************
 * CM_Is_Dock_Station_Present_Ex [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Is_Dock_Station_Present_Ex(
    PBOOL pbPresent, HMACHINE hMachine)
{
    RPC_BINDING_HANDLE BindingHandle = NULL;

    TRACE("%p %lx\n", pbPresent, hMachine);

    if (pbPresent == NULL)
        return CR_INVALID_POINTER;

    *pbPresent = FALSE;

    if (hMachine != NULL)
    {
        BindingHandle = ((PMACHINE_INFO)hMachine)->BindingHandle;
        if (BindingHandle == NULL)
            return CR_FAILURE;
    }
    else
    {
        if (!PnpGetLocalHandles(&BindingHandle, NULL))
            return CR_FAILURE;
    }

    return PNP_IsDockStationPresent(BindingHandle,
                                    (unsigned long *)pbPresent);
}


/***********************************************************************
 * CM_Locate_DevNodeA [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Locate_DevNodeA(
    PDEVINST pdnDevInst, DEVINSTID_A pDeviceID, ULONG ulFlags)
{
    TRACE("%p %s %lu\n", pdnDevInst, pDeviceID, ulFlags);
    return CM_Locate_DevNode_ExA(pdnDevInst, pDeviceID, ulFlags, NULL);
}


/***********************************************************************
 * CM_Locate_DevNodeW [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Locate_DevNodeW(
    PDEVINST pdnDevInst, DEVINSTID_W pDeviceID, ULONG ulFlags)
{
    TRACE("%p %s %lu\n", pdnDevInst, debugstr_w(pDeviceID), ulFlags);
    return CM_Locate_DevNode_ExW(pdnDevInst, pDeviceID, ulFlags, NULL);
}


/***********************************************************************
 * CM_Locate_DevNode_ExA [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Locate_DevNode_ExA(
    PDEVINST pdnDevInst, DEVINSTID_A pDeviceID, ULONG ulFlags, HMACHINE hMachine)
{
    DEVINSTID_W pDevIdW = NULL;
    CONFIGRET ret = CR_SUCCESS;

    TRACE("%p %s %lu %lx\n", pdnDevInst, pDeviceID, ulFlags, hMachine);

    if (pDeviceID != NULL)
    {
       if (CaptureAndConvertAnsiArg(pDeviceID, &pDevIdW))
         return CR_INVALID_DEVICE_ID;
    }

    ret = CM_Locate_DevNode_ExW(pdnDevInst, pDevIdW, ulFlags, hMachine);

    if (pDevIdW != NULL)
        MyFree(pDevIdW);

    return ret;
}


/***********************************************************************
 * CM_Locate_DevNode_ExW [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Locate_DevNode_ExW(
    PDEVINST pdnDevInst, DEVINSTID_W pDeviceID, ULONG ulFlags, HMACHINE hMachine)
{
    WCHAR DeviceIdBuffer[MAX_DEVICE_ID_LEN];
    RPC_BINDING_HANDLE BindingHandle = NULL;
    HSTRING_TABLE StringTable = NULL;
    CONFIGRET ret = CR_SUCCESS;

    TRACE("%p %s %lu %lx\n", pdnDevInst, debugstr_w(pDeviceID), ulFlags, hMachine);

    if (pdnDevInst == NULL)
        return CR_INVALID_POINTER;

    if (ulFlags & ~CM_LOCATE_DEVNODE_BITS)
        return CR_INVALID_FLAG;

    if (hMachine != NULL)
    {
        BindingHandle = ((PMACHINE_INFO)hMachine)->BindingHandle;
        if (BindingHandle == NULL)
            return CR_FAILURE;

        StringTable = ((PMACHINE_INFO)hMachine)->StringTable;
        if (StringTable == 0)
            return CR_FAILURE;
    }
    else
    {
        if (!PnpGetLocalHandles(&BindingHandle, &StringTable))
            return CR_FAILURE;
    }

    if (pDeviceID != NULL && lstrlenW(pDeviceID) != 0)
    {
        lstrcpyW(DeviceIdBuffer, pDeviceID);
    }
    else
    {
        /* Get the root device ID */
        ret = PNP_GetRootDeviceInstance(BindingHandle,
                                        DeviceIdBuffer,
                                        MAX_DEVICE_ID_LEN);
        if (ret != CR_SUCCESS)
            return CR_FAILURE;
    }
    TRACE("DeviceIdBuffer: %s\n", debugstr_w(DeviceIdBuffer));

    /* Validate the device ID */
    ret = PNP_ValidateDeviceInstance(BindingHandle,
                                     DeviceIdBuffer,
                                     ulFlags);
    if (ret == CR_SUCCESS)
    {
        *pdnDevInst = StringTableAddString(StringTable, DeviceIdBuffer, 1);
        if (*pdnDevInst == -1)
            ret = CR_FAILURE;
    }

    return ret;
}


/***********************************************************************
 * CM_Move_DevNode [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Move_DevNode(
    DEVINST dnFromDevInst, DEVINST dnToDevInst, ULONG ulFlags)
{
    TRACE("%lx %lx %lx\n", dnFromDevInst, dnToDevInst, ulFlags);
    return CM_Move_DevNode_Ex(dnFromDevInst, dnToDevInst, ulFlags, NULL);
}


/***********************************************************************
 * CM_Move_DevNode_Ex [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Move_DevNode_Ex(
    DEVINST dnFromDevInst, DEVINST dnToDevInst, ULONG ulFlags,
    HMACHINE hMachine)
{
    RPC_BINDING_HANDLE BindingHandle = NULL;
    HSTRING_TABLE StringTable = NULL;
    LPWSTR lpFromDevInst;
    LPWSTR lpToDevInst;

    FIXME("%lx %lx %lx %lx\n",
          dnFromDevInst, dnToDevInst, ulFlags, hMachine);

    if (!IsUserAdmin())
        return CR_ACCESS_DENIED;

    if (dnFromDevInst == 0 || dnToDevInst == 0)
        return CR_INVALID_DEVNODE;

    if (ulFlags != 0)
        return CR_INVALID_FLAG;

    if (hMachine != NULL)
    {
        BindingHandle = ((PMACHINE_INFO)hMachine)->BindingHandle;
        if (BindingHandle == NULL)
            return CR_FAILURE;

        StringTable = ((PMACHINE_INFO)hMachine)->StringTable;
        if (StringTable == 0)
            return CR_FAILURE;
    }
    else
    {
        if (!PnpGetLocalHandles(&BindingHandle, &StringTable))
            return CR_FAILURE;
    }

    lpFromDevInst = StringTableStringFromId(StringTable, dnFromDevInst);
    if (lpFromDevInst == NULL)
        return CR_INVALID_DEVNODE;

    lpToDevInst = StringTableStringFromId(StringTable, dnToDevInst);
    if (lpToDevInst == NULL)
        return CR_INVALID_DEVNODE;

    return PNP_DeviceInstanceAction(BindingHandle,
                                    2,
                                    ulFlags,
                                    lpFromDevInst,
                                    lpToDevInst);
}


/***********************************************************************
 * CM_Open_Class_KeyA [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Open_Class_KeyA(
    LPGUID pClassGuid, LPCSTR pszClassName, REGSAM samDesired,
    REGDISPOSITION Disposition, PHKEY phkClass, ULONG ulFlags)
{
    TRACE("%p %s %lx %lx %p %lx\n",
          debugstr_guid(pClassGuid), pszClassName,
          samDesired, Disposition, phkClass, ulFlags);

    return CM_Open_Class_Key_ExA(pClassGuid, pszClassName, samDesired,
                                 Disposition, phkClass, ulFlags, NULL);
}


/***********************************************************************
 * CM_Open_Class_KeyW [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Open_Class_KeyW(
    LPGUID pClassGuid, LPCWSTR pszClassName, REGSAM samDesired,
    REGDISPOSITION Disposition, PHKEY phkClass, ULONG ulFlags)
{
    TRACE("%p %s %lx %lx %p %lx\n",
          debugstr_guid(pClassGuid), debugstr_w(pszClassName),
          samDesired, Disposition, phkClass, ulFlags);

    return CM_Open_Class_Key_ExW(pClassGuid, pszClassName, samDesired,
                                 Disposition, phkClass, ulFlags, NULL);
}


/***********************************************************************
 * CM_Open_Class_Key_ExA [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Open_Class_Key_ExA(
    LPGUID pClassGuid, LPCSTR pszClassName, REGSAM samDesired,
    REGDISPOSITION Disposition, PHKEY phkClass, ULONG ulFlags,
    HMACHINE hMachine)
{
    CONFIGRET rc = CR_SUCCESS;
    LPWSTR pszClassNameW = NULL;

    TRACE("%p %s %lx %lx %p %lx %lx\n",
          debugstr_guid(pClassGuid), pszClassName,
          samDesired, Disposition, phkClass, ulFlags, hMachine);

    if (pszClassName != NULL)
    {
       if (CaptureAndConvertAnsiArg(pszClassName, &pszClassNameW))
         return CR_INVALID_DATA;
    }

    rc = CM_Open_Class_Key_ExW(pClassGuid, pszClassNameW, samDesired,
                               Disposition, phkClass, ulFlags, hMachine);

    if (pszClassNameW != NULL)
        MyFree(pszClassNameW);

    return CR_SUCCESS;
}


/***********************************************************************
 * CM_Open_Class_Key_ExW [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Open_Class_Key_ExW(
    LPGUID pClassGuid, LPCWSTR pszClassName, REGSAM samDesired,
    REGDISPOSITION Disposition, PHKEY phkClass, ULONG ulFlags,
    HMACHINE hMachine)
{
    WCHAR szKeyName[MAX_PATH];
    LPWSTR lpGuidString;
    DWORD dwDisposition;
    DWORD dwError;
    HKEY hKey;

    TRACE("%p %s %lx %lx %p %lx %lx\n",
          debugstr_guid(pClassGuid), debugstr_w(pszClassName),
          samDesired, Disposition, phkClass, ulFlags, hMachine);

    /* Check Disposition and ulFlags */
    if ((Disposition & ~RegDisposition_Bits) ||
        (ulFlags & ~CM_OPEN_CLASS_KEY_BITS))
        return CR_INVALID_FLAG;

    /* Check phkClass */
    if (phkClass == NULL)
        return CR_INVALID_POINTER;

    *phkClass = NULL;

    if (ulFlags == CM_OPEN_CLASS_KEY_INTERFACE &&
        pszClassName != NULL)
        return CR_INVALID_DATA;

    if (hMachine == NULL)
    {
        hKey = HKEY_LOCAL_MACHINE;
    }
    else
    {
       if (RegConnectRegistryW(((PMACHINE_INFO)hMachine)->szMachineName,
                               HKEY_LOCAL_MACHINE, &hKey))
           return CR_REGISTRY_ERROR;
    }

    if (ulFlags & CM_OPEN_CLASS_KEY_INTERFACE)
    {
        lstrcpyW(szKeyName, DeviceClasses);
    }
    else
    {
        lstrcpyW(szKeyName, ControlClass);
    }

    if (pClassGuid != NULL)
    {
        if (UuidToStringW((UUID*)pClassGuid, &lpGuidString) != RPC_S_OK)
        {
            RegCloseKey(hKey);
            return CR_INVALID_DATA;
        }

        lstrcatW(szKeyName, Backslash);
        lstrcatW(szKeyName, lpGuidString);
    }

    if (Disposition == RegDisposition_OpenAlways)
    {
        dwError = RegCreateKeyExW(hKey, szKeyName, 0, NULL, 0, samDesired,
                                  NULL, phkClass, &dwDisposition);
    }
    else
    {
        dwError = RegOpenKeyExW(hKey, szKeyName, 0, samDesired, phkClass);
    }

    RegCloseKey(hKey);

    if (pClassGuid != NULL)
        RpcStringFreeW(&lpGuidString);

    if (dwError != ERROR_SUCCESS)
    {
        *phkClass = NULL;
        return CR_NO_SUCH_REGISTRY_KEY;
    }

    if (pszClassName != NULL)
    {
        RegSetValueExW(*phkClass, Class, 0, REG_SZ, (LPBYTE)pszClassName,
                       (lstrlenW(pszClassName) + 1) * sizeof(WCHAR));
    }

    return CR_SUCCESS;
}


/***********************************************************************
 * CM_Reenumerate_DevNode [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Reenumerate_DevNode(
    DEVINST dnDevInst, ULONG ulFlags)
{
    TRACE("%lx %lx\n", dnDevInst, ulFlags);
    return CM_Reenumerate_DevNode_Ex(dnDevInst, ulFlags, NULL);
}


/***********************************************************************
 * CM_Reenumerate_DevNode_Ex [SETUPAPI.@]
 */
CONFIGRET WINAPI
CM_Reenumerate_DevNode_Ex(
    DEVINST dnDevInst, ULONG ulFlags, HMACHINE hMachine)
{
    RPC_BINDING_HANDLE BindingHandle = NULL;
    HSTRING_TABLE StringTable = NULL;
    LPWSTR lpDevInst;

    FIXME("%lx %lx %lx\n", dnDevInst, ulFlags, hMachine);

    if (dnDevInst == 0)
        return CR_INVALID_DEVNODE;

    if (ulFlags & ~CM_REENUMERATE_BITS)
        return CR_INVALID_FLAG;

    if (hMachine != NULL)
    {
        BindingHandle = ((PMACHINE_INFO)hMachine)->BindingHandle;
        if (BindingHandle == NULL)
            return CR_FAILURE;

        StringTable = ((PMACHINE_INFO)hMachine)->StringTable;
        if (StringTable == 0)
            return CR_FAILURE;
    }
    else
    {
        if (!PnpGetLocalHandles(&BindingHandle, &StringTable))
            return CR_FAILURE;
    }

    lpDevInst = StringTableStringFromId(StringTable, dnDevInst);
    if (lpDevInst == NULL)
        return CR_INVALID_DEVNODE;

    return PNP_DeviceInstanceAction(BindingHandle,
                                    7,
                                    ulFlags,
                                    lpDevInst,
                                    NULL);
}


/***********************************************************************
 * CM_Request_Eject_PC [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Request_Eject_PC(VOID)
{
    TRACE("\n");
    return CM_Request_Eject_PC_Ex(NULL);
}


/***********************************************************************
 * CM_Request_Eject_PC_Ex [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Request_Eject_PC_Ex(
    HMACHINE hMachine)
{
    RPC_BINDING_HANDLE BindingHandle = NULL;

    TRACE("%lx\n", hMachine);

    if (hMachine != NULL)
    {
        BindingHandle = ((PMACHINE_INFO)hMachine)->BindingHandle;
        if (BindingHandle == NULL)
            return CR_FAILURE;
    }
    else
    {
        if (!PnpGetLocalHandles(&BindingHandle, NULL))
            return CR_FAILURE;
    }

    return PNP_RequestEjectPC(BindingHandle);
}


/***********************************************************************
 * CM_Run_Detection [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Run_Detection(
    ULONG ulFlags)
{
    TRACE("%lx\n", ulFlags);
    return CM_Run_Detection_Ex(ulFlags, NULL);
}


/***********************************************************************
 * CM_Run_Detection_Ex [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Run_Detection_Ex(
    ULONG ulFlags, HMACHINE hMachine)
{
    RPC_BINDING_HANDLE BindingHandle = NULL;

    TRACE("%lx %lx\n", ulFlags, hMachine);

    if (!IsUserAdmin())
        return CR_ACCESS_DENIED;

    if (ulFlags & ~CM_DETECT_BITS)
        return CR_INVALID_FLAG;

    if (hMachine != NULL)
    {
        BindingHandle = ((PMACHINE_INFO)hMachine)->BindingHandle;
        if (BindingHandle == NULL)
            return CR_FAILURE;
    }
    else
    {
        if (!PnpGetLocalHandles(&BindingHandle, NULL))
            return CR_FAILURE;
    }

    return PNP_RunDetection(BindingHandle,
                            ulFlags);
}


/***********************************************************************
 * CM_Set_DevNode_Problem [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Set_DevNode_Problem(
    DEVINST dnDevInst, ULONG ulProblem, ULONG ulFlags)
{
    TRACE("%lx %lx %lx\n", dnDevInst, ulProblem, ulFlags);
    return CM_Set_DevNode_Problem_Ex(dnDevInst, ulProblem, ulFlags, NULL);
}


/***********************************************************************
 * CM_Set_DevNode_Problem_Ex [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Set_DevNode_Problem_Ex(
    DEVINST dnDevInst, ULONG ulProblem, ULONG ulFlags, HMACHINE hMachine)
{
    RPC_BINDING_HANDLE BindingHandle = NULL;
    HSTRING_TABLE StringTable = NULL;
    LPWSTR lpDevInst;

    TRACE("%lx %lx %lx %lx\n", dnDevInst, ulProblem, ulFlags, hMachine);

    if (dnDevInst == 0)
        return CR_INVALID_DEVNODE;

    if (ulFlags & ~CM_SET_DEVNODE_PROBLEM_BITS)
        return CR_INVALID_FLAG;

    if (hMachine != NULL)
    {
        BindingHandle = ((PMACHINE_INFO)hMachine)->BindingHandle;
        if (BindingHandle == NULL)
            return CR_FAILURE;

        StringTable = ((PMACHINE_INFO)hMachine)->StringTable;
        if (StringTable == 0)
            return CR_FAILURE;
    }
    else
    {
        if (!PnpGetLocalHandles(&BindingHandle, &StringTable))
            return CR_FAILURE;
    }

    lpDevInst = StringTableStringFromId(StringTable, dnDevInst);
    if (lpDevInst == NULL)
        return CR_INVALID_DEVNODE;

    return PNP_SetDeviceProblem(BindingHandle,
                                lpDevInst,
                                ulProblem,
                                ulFlags);
}


/***********************************************************************
 * CM_Set_DevNode_Registry_PropertyA [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Set_DevNode_Registry_PropertyA(
    DEVINST dnDevInst, ULONG ulProperty, PCVOID Buffer, ULONG ulLength,
    ULONG ulFlags)
{
    TRACE("%lx %lu %p %lx %lx\n",
          dnDevInst, ulProperty, Buffer, ulLength, ulFlags);
    return CM_Set_DevNode_Registry_Property_ExA(dnDevInst, ulProperty,
                                                Buffer, ulLength,
                                                ulFlags, NULL);
}


/***********************************************************************
 * CM_Set_DevNode_Registry_PropertyW [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Set_DevNode_Registry_PropertyW(
    DEVINST dnDevInst, ULONG ulProperty, PCVOID Buffer, ULONG ulLength,
    ULONG ulFlags)
{
    TRACE("%lx %lu %p %lx %lx\n",
          dnDevInst, ulProperty, Buffer, ulLength, ulFlags);
    return CM_Set_DevNode_Registry_Property_ExW(dnDevInst, ulProperty,
                                                Buffer, ulLength,
                                                ulFlags, NULL);
}


/***********************************************************************
 * CM_Set_DevNode_Registry_Property_ExA [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Set_DevNode_Registry_Property_ExA(
    DEVINST dnDevInst, ULONG ulProperty, PCVOID Buffer, ULONG ulLength,
    ULONG ulFlags, HMACHINE hMachine)
{
    CONFIGRET ret = CR_SUCCESS;
    LPWSTR lpBuffer;
    ULONG ulType;

    FIXME("%lx %lu %p %lx %lx %lx\n",
          dnDevInst, ulProperty, Buffer, ulLength, ulFlags, hMachine);

    if (Buffer == NULL && ulLength != 0)
        return CR_INVALID_POINTER;

    if (Buffer == NULL)
    {
        ret = CM_Set_DevNode_Registry_Property_ExW(dnDevInst,
                                                   ulProperty,
                                                   NULL,
                                                   0,
                                                   ulFlags,
                                                   hMachine);
    }
    else
    {
        /* Get property type */
        switch (ulProperty)
        {
            case CM_DRP_DEVICEDESC:
                ulType = REG_SZ;
                break;

            case CM_DRP_HARDWAREID:
                ulType = REG_MULTI_SZ;
                break;

            case CM_DRP_COMPATIBLEIDS:
                ulType = REG_MULTI_SZ;
                break;

            case CM_DRP_SERVICE:
                ulType = REG_SZ;
                break;

            case CM_DRP_CLASS:
                ulType = REG_SZ;
                break;

            case CM_DRP_CLASSGUID:
                ulType = REG_SZ;
                break;

            case CM_DRP_DRIVER:
                ulType = REG_SZ;
                break;

            case CM_DRP_CONFIGFLAGS:
                ulType = REG_DWORD;
                break;

            case CM_DRP_MFG:
                ulType = REG_SZ;
                break;

            case CM_DRP_FRIENDLYNAME:
                ulType = REG_SZ;
                break;

            case CM_DRP_LOCATION_INFORMATION:
                ulType = REG_SZ;
                break;

            case CM_DRP_UPPERFILTERS:
                ulType = REG_MULTI_SZ;
                break;

            case CM_DRP_LOWERFILTERS:
                ulType = REG_MULTI_SZ;
                break;

            default:
                return CR_INVALID_PROPERTY;
        }

        /* Allocate buffer if needed */
        if (ulType == REG_SZ ||
            ulType == REG_MULTI_SZ)
        {
            lpBuffer = MyMalloc(ulLength * sizeof(WCHAR));
            if (lpBuffer == NULL)
            {
                ret = CR_OUT_OF_MEMORY;
            }
            else
            {
                if (!MultiByteToWideChar(CP_ACP, 0, Buffer,
                                         ulLength, lpBuffer, ulLength))
                {
                    MyFree(lpBuffer);
                    ret = CR_FAILURE;
                }
                else
                {
                    ret = CM_Set_DevNode_Registry_Property_ExW(dnDevInst,
                                                               ulProperty,
                                                               lpBuffer,
                                                               ulLength * sizeof(WCHAR),
                                                               ulFlags,
                                                               hMachine);
                    MyFree(lpBuffer);
                }
            }
        }
        else
        {
            ret = CM_Set_DevNode_Registry_Property_ExW(dnDevInst,
                                                       ulProperty,
                                                       Buffer,
                                                       ulLength,
                                                       ulFlags,
                                                       hMachine);
        }

        ret = CR_CALL_NOT_IMPLEMENTED;
    }

    return ret;
}


/***********************************************************************
 * CM_Set_DevNode_Registry_Property_ExW [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Set_DevNode_Registry_Property_ExW(
    DEVINST dnDevInst, ULONG ulProperty, PCVOID Buffer, ULONG ulLength,
    ULONG ulFlags, HMACHINE hMachine)
{
    RPC_BINDING_HANDLE BindingHandle = NULL;
    HSTRING_TABLE StringTable = NULL;
    LPWSTR lpDevInst;
    ULONG ulType;

    TRACE("%lx %lu %p %lx %lx %lx\n",
          dnDevInst, ulProperty, Buffer, ulLength, ulFlags, hMachine);

    if (dnDevInst == 0)
        return CR_INVALID_DEVNODE;

    if (ulProperty <  CM_DRP_MIN || ulProperty > CM_DRP_MAX)
        return CR_INVALID_PROPERTY;

    if (Buffer != NULL && ulLength == 0)
        return CR_INVALID_POINTER;

    if (ulFlags != 0)
        return CR_INVALID_FLAG;

    if (hMachine != NULL)
    {
        BindingHandle = ((PMACHINE_INFO)hMachine)->BindingHandle;
        if (BindingHandle == NULL)
            return CR_FAILURE;

        StringTable = ((PMACHINE_INFO)hMachine)->StringTable;
        if (StringTable == 0)
            return CR_FAILURE;
    }
    else
    {
        if (!PnpGetLocalHandles(&BindingHandle, &StringTable))
            return CR_FAILURE;
    }

    lpDevInst = StringTableStringFromId(StringTable, dnDevInst);
    if (lpDevInst == NULL)
        return CR_INVALID_DEVNODE;

    switch (ulProperty)
    {
        case CM_DRP_DEVICEDESC:
            ulType = REG_SZ;
            break;

        case CM_DRP_HARDWAREID:
            ulType = REG_MULTI_SZ;
            break;

        case CM_DRP_COMPATIBLEIDS:
            ulType = REG_MULTI_SZ;
            break;

        case CM_DRP_SERVICE:
            ulType = REG_SZ;
            break;

        case CM_DRP_CLASS:
            ulType = REG_SZ;
            break;

        case CM_DRP_CLASSGUID:
            ulType = REG_SZ;
            break;

        case CM_DRP_DRIVER:
            ulType = REG_SZ;
            break;

        case CM_DRP_CONFIGFLAGS:
            ulType = REG_DWORD;
            break;

        case CM_DRP_MFG:
            ulType = REG_SZ;
            break;

        case CM_DRP_FRIENDLYNAME:
            ulType = REG_SZ;
            break;

        case CM_DRP_LOCATION_INFORMATION:
            ulType = REG_SZ;
            break;

        case CM_DRP_UPPERFILTERS:
            ulType = REG_MULTI_SZ;
            break;

        case CM_DRP_LOWERFILTERS:
            ulType = REG_MULTI_SZ;
            break;

        default:
            return CR_INVALID_PROPERTY;
    }

    return PNP_SetDeviceRegProp(BindingHandle,
                                lpDevInst,
                                ulProperty,
                                ulType,
                                (char *)Buffer,
                                ulLength,
                                ulFlags);
}


/***********************************************************************
 * CM_Setup_DevNode [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Setup_DevNode(
    DEVINST dnDevInst, ULONG ulFlags)
{
    TRACE("%lx %lx\n", dnDevInst, ulFlags);
    return CM_Setup_DevNode_Ex(dnDevInst, ulFlags, NULL);
}


/***********************************************************************
 * CM_Setup_DevNode_Ex [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Setup_DevNode_Ex(
    DEVINST dnDevInst, ULONG ulFlags, HMACHINE hMachine)
{
    RPC_BINDING_HANDLE BindingHandle = NULL;
    HSTRING_TABLE StringTable = NULL;
    LPWSTR lpDevInst;

    FIXME("%lx %lx %lx\n", dnDevInst, ulFlags, hMachine);

    if (!IsUserAdmin())
        return CR_ACCESS_DENIED;

    if (dnDevInst == 0)
        return CR_INVALID_DEVNODE;

    if (ulFlags & ~CM_SETUP_BITS)
        return CR_INVALID_FLAG;

    if (hMachine != NULL)
    {
        BindingHandle = ((PMACHINE_INFO)hMachine)->BindingHandle;
        if (BindingHandle == NULL)
            return CR_FAILURE;

        StringTable = ((PMACHINE_INFO)hMachine)->StringTable;
        if (StringTable == 0)
            return CR_FAILURE;
    }
    else
    {
        if (!PnpGetLocalHandles(&BindingHandle, &StringTable))
            return CR_FAILURE;
    }

    lpDevInst = StringTableStringFromId(StringTable, dnDevInst);
    if (lpDevInst == NULL)
        return CR_INVALID_DEVNODE;

    return PNP_DeviceInstanceAction(BindingHandle,
                                    3,
                                    ulFlags,
                                    lpDevInst,
                                    NULL);
}


/***********************************************************************
 * CM_Uninstall_DevNode [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Uninstall_DevNode(
    DEVINST dnPhantom, ULONG ulFlags)
{
    TRACE("%lx %lx\n", dnPhantom, ulFlags);
    return CM_Uninstall_DevNode_Ex(dnPhantom, ulFlags, NULL);
}


/***********************************************************************
 * CM_Uninstall_DevNode_Ex [SETUPAPI.@]
 */
CONFIGRET WINAPI CM_Uninstall_DevNode_Ex(
    DEVINST dnPhantom, ULONG ulFlags, HMACHINE hMachine)
{
    RPC_BINDING_HANDLE BindingHandle = NULL;
    HSTRING_TABLE StringTable = NULL;
    LPWSTR lpDevInst;

    TRACE("%lx %lx %lx\n", dnPhantom, ulFlags, hMachine);

    if (dnPhantom == 0)
        return CR_INVALID_DEVNODE;

    if (ulFlags != 0)
        return CR_INVALID_FLAG;

    if (hMachine != NULL)
    {
        BindingHandle = ((PMACHINE_INFO)hMachine)->BindingHandle;
        if (BindingHandle == NULL)
            return CR_FAILURE;

        StringTable = ((PMACHINE_INFO)hMachine)->StringTable;
        if (StringTable == 0)
            return CR_FAILURE;
    }
    else
    {
        if (!PnpGetLocalHandles(&BindingHandle, &StringTable))
            return CR_FAILURE;
    }

    lpDevInst = StringTableStringFromId(StringTable, dnPhantom);
    if (lpDevInst == NULL)
        return CR_INVALID_DEVNODE;

    return PNP_UninstallDevInst(BindingHandle,
                                lpDevInst,
                                ulFlags);
}
