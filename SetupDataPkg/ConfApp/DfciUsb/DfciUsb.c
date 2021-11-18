/** @file
DfciUsb.c

This module will request new DFCI configuration data from a USB drive.

Copyright (C) Microsoft Corporation. All rights reserved.
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Guid/DfciPacketHeader.h>

#include <Protocol/DevicePath.h>
#include <Protocol/SimpleFileSystem.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/FileHandleLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PrintLib.h>

#include "DfciUtility.h"
#include "DfciUsb.h"

/**
 * BuildUsbRequest
 *
 * @param[in]   FileNameExtension - Extension for file name
 * @param[out]  filename          - Name of the file on USB to retrieve
 *
 **/
EFI_STATUS
EFIAPI
BuildUsbRequest (
    IN  CHAR16       *FileExtension,
    OUT CHAR16      **FileName
  ) {

    DFCI_SYSTEM_INFORMATION  DfciInfo;
    UINTN                    i;
    CHAR16                  *PktFileName = NULL;
    UINTN                    PktNameLen;
    EFI_STATUS               Status;

    PktFileName = NULL;

    Status = DfciGetSystemInfo (&DfciInfo);
    if (EFI_ERROR(Status)) {
         goto Error;
    }

    PktFileName = (CHAR16 *) AllocatePool (MAX_USB_FILE_NAME_LENGTH * sizeof(CHAR16));
    if (PktFileName == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto Error;
    }

    // The maximum file name length is 255 characters and a NULL.  Leave room for
    // the four character file name extension.  Create the base PktFileName out of
    // the first 251 characters of SerialNumber_ProductName_Manufacturer then add the
    // file name extension.

    PktNameLen = UnicodeSPrintAsciiFormat (PktFileName,
                                          (MAX_USB_FILE_NAME_LENGTH - 4) * sizeof(CHAR16),
                                          "%a_%a_%a",
                                           DfciInfo.SerialNumber,
                                           DfciInfo.ProductName,
                                           DfciInfo.Manufacturer);
    DfciFreeSystemInfo (&DfciInfo);
    if ((PktNameLen == 0) || (PktNameLen >= (MAX_USB_FILE_NAME_LENGTH - 4))) {
        DEBUG((DEBUG_ERROR, "Invalid file name length %d\n", PktNameLen));
        Status = EFI_BAD_BUFFER_SIZE;
        goto Error;
    }

    //
    //  Any binary value of 0x01-0x1f, and any of    " * / : < > ? \ |
    //  are not allowed in the file name.  If any of these exist, then
    //  replace the invalid character with an '@'.
    //
    for (i = 0; i < PktNameLen; i++) {
        if (((PktFileName[i] >= 0x00) &&
             (PktFileName[i] <= 0x1F)) ||
             (PktFileName[i] == L'\"') ||
             (PktFileName[i] == L'*')  ||
             (PktFileName[i] == L'/')  ||
             (PktFileName[i] == L':')  ||
             (PktFileName[i] == L'<')  ||
             (PktFileName[i] == L'>')  ||
             (PktFileName[i] == L'?')  ||
             (PktFileName[i] == L'\\') ||
             (PktFileName[i] == L'|')) {
            PktFileName[i] = L'@';
        }
    }

    Status = StrCatS (PktFileName, MAX_USB_FILE_NAME_LENGTH * sizeof(CHAR16), FileExtension);
    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR, "Unable to append the file name ext. Code=%r\n", Status));
        goto Error;
    }


    *FileName = PktFileName;

    return EFI_SUCCESS;

Error:
    if (NULL != PktFileName) {
       FreePool (PktFileName);
    }

    DfciFreeSystemInfo (&DfciInfo);

    return Status;
}

/**
*
*  Scan USB Drives looking for the file name passed in.
*
*  @param[in]     PktFileName     Name of update file to read.
*  @param[out]    Buffer          Where to store a buffer pointer.
*  @param[out]    BufferSize      Where to store the buffer size.
*
*
*  @retval   EFI_SUCCESS     The FS volume was opened successfully.
*  @retval   Others          The operation failed.
*
**/
STATIC
EFI_STATUS
FindUsbDriveWithDfciUpdate (
    IN  CHAR16   *PktFileName,
    OUT CHAR8   **Buffer,
    OUT UINTN    *BufferSize
  ) {

    EFI_FILE_PROTOCOL               *FileHandle;
    EFI_FILE_PROTOCOL               *VolHandle;
    EFI_HANDLE                      *HandleBuffer;
    UINTN                            Index;
    UINTN                            NumHandles;
    EFI_STATUS                       Status;
    EFI_STATUS                       Status2;
    EFI_DEVICE_PATH_PROTOCOL        *BlkIoDevicePath;
    EFI_DEVICE_PATH_PROTOCOL        *UsbDevicePath;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *SfProtocol;
    EFI_HANDLE                       Handle;
    EFI_FILE_INFO                   *FileInfo;
    UINTN                            ReadSize;


    if ((NULL == PktFileName) ||
        (NULL == Buffer) ||
        (NULL == BufferSize)) {
        return EFI_INVALID_PARAMETER;
    }

    NumHandles = 0;
    HandleBuffer = NULL;
    SfProtocol = NULL;

    //
    // Locate all handles that are using the SFS protocol.
    //
    Status = gBS->LocateHandleBuffer(ByProtocol,
                                     &gEfiSimpleFileSystemProtocolGuid,
                                     NULL,
                                     &NumHandles,
                                     &HandleBuffer);

    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR, "%a: failed to locate any handles using the Simple FS protocol (%r)\n", __FUNCTION__, Status));
        goto CleanUp;
    }

    DEBUG((DEBUG_INFO,"Processing %d handles\n", NumHandles));

    //
    // Search the handles to find one that has has a USB node in the device path.
    //
    for (Index = 0; (Index < NumHandles); Index += 1) {
        //
        // Ensure this device is on a USB controller
        //
        UsbDevicePath = DevicePathFromHandle(HandleBuffer[Index]);
        if (UsbDevicePath == NULL) {
            DEBUG((DEBUG_ERROR,"No device path on handle %d\n", Index));
            continue;
        }
        Status = gBS->LocateDevicePath (&gEfiUsbIoProtocolGuid,
                                        &UsbDevicePath,
                                        &Handle);
        if (EFI_ERROR(Status)) {
            // Device is not USB;
            DEBUG((DEBUG_ERROR,"Not a USB Device on Handle %d\n", Index));
            continue;
        }

        //
        // Check if this is a block IO device path.
        //
        BlkIoDevicePath = DevicePathFromHandle(HandleBuffer[Index]);
        if (BlkIoDevicePath == NULL) {
            DEBUG((DEBUG_ERROR,"This cannot happen on %d\n", Index));
            continue;
        }
        Status = gBS->LocateDevicePath(&gEfiBlockIoProtocolGuid,
                                       &BlkIoDevicePath,
                                       &Handle);
        if (EFI_ERROR(Status)) {
            // Device is not BlockIo;
            DEBUG((DEBUG_ERROR,"Not a BlockIo Device on Handle %d\n", Index));
            continue;
        }

        Status = gBS->HandleProtocol(HandleBuffer[Index],
                                     &gEfiSimpleFileSystemProtocolGuid,
                                     (VOID**)&SfProtocol);

        if (EFI_ERROR(Status)) {
            DEBUG((DEBUG_ERROR, "%a: Failed to locate Simple FS protocol. %r\n", __FUNCTION__, Status));
            continue;
        }

        //
        // Open the volume/partition.
        //
        Status = SfProtocol->OpenVolume(SfProtocol, &VolHandle);
        if (EFI_ERROR(Status) != FALSE) {
            DEBUG((DEBUG_ERROR,"%a: Unable to open SimpleFileSystem. Code = %r\n", __FUNCTION__, Status));
            continue;
        }

        //
        // Ensure the PktName file is present
        //
        Status = VolHandle->Open (VolHandle, &FileHandle, PktFileName, EFI_FILE_MODE_READ, 0);
        if (EFI_ERROR(Status)) {
            DEBUG((DEBUG_INFO,"%a: Unable to locate %s. Code = %r\n", __FUNCTION__, PktFileName, Status));
            Status2 = FileHandleClose (VolHandle);
            if (EFI_ERROR(Status2)) {
                DEBUG((DEBUG_ERROR,"%a: Error closing Vol Handle. Code = %r\n", __FUNCTION__, Status2));
            }
            continue;
        }

        FileInfo = FileHandleGetInfo (FileHandle);

        if (FileInfo == NULL) {
            DEBUG((DEBUG_ERROR,"%a: Error getting file info.\n", __FUNCTION__));
            FileHandleClose (FileHandle);
            FileHandleClose (VolHandle);
            continue;
        }

        //
        // There can be six items encoded in base64 (4 ascii bytes per 3
        // binary bytes) + some overhead for the json structure (64 bytes
        // for each of the 6 entries).
        //
        if ((FileInfo->FileSize == 0) ||
            (FileInfo->FileSize > ((MAX_ALLOWABLE_DFCI_APPLY_VAR_SIZE * 6 * 4) / 3 + 384))) {
            DEBUG((DEBUG_ERROR,"%a: Invalid file size %d.\n", __FUNCTION__, FileInfo->FileSize));
            Status = EFI_BAD_BUFFER_SIZE;
            FileHandleClose (FileHandle);
            FileHandleClose (VolHandle);
            continue;
        }

        *Buffer = AllocatePool (FileInfo->FileSize + sizeof(CHAR8));  // Add 1 for a terminating NULL
        if (*Buffer == NULL) {
            FileHandleClose (FileHandle);
            FileHandleClose (VolHandle);
            DEBUG((DEBUG_ERROR,"%a: Unable to allocate buffer.\n", __FUNCTION__));
            Status = EFI_OUT_OF_RESOURCES;
            goto CleanUp; // Fatal error, don't try anymore
        }
        *BufferSize = FileInfo->FileSize + sizeof(CHAR8);

        DEBUG((DEBUG_INFO,"Reading file into buffer @ %p, size = %d\n",*Buffer, *BufferSize));

        ReadSize = FileInfo->FileSize;
        Status = FileHandleRead (FileHandle, &ReadSize, *Buffer);
        if (EFI_ERROR(Status) || (ReadSize != FileInfo->FileSize)) {
            FileHandleClose (FileHandle);
            FileHandleClose (VolHandle);
            DEBUG((DEBUG_ERROR,"%a: Unable to read file. ReadSize=%d, Size=%d. Code=%r\n",
                               __FUNCTION__,
                               ReadSize,
                               FileInfo->FileSize,
                               Status));

            if (Status == EFI_SUCCESS) {
                Status = EFI_BAD_BUFFER_SIZE;
            }
            FreePool (*Buffer);
            *Buffer = NULL;
            continue;
        }
        (*Buffer)[FileInfo->FileSize] = '\0';  // Add a terminating NULL
        DEBUG((DEBUG_INFO,"Finished Reading File\n"));
        FileHandleClose (FileHandle);
        FileHandleClose (VolHandle);
        Status = EFI_SUCCESS;
        break;
    }

CleanUp:
    if (HandleBuffer != NULL) {
        FreePool(HandleBuffer);
    }

    DEBUG((DEBUG_INFO,"Exit reading file\n"));

    return Status;
}

/**
*
*  Request a Json Dfci settings packet.
*
*  @param[in]     FileName        What file to read.
*  @param[out]    JsonString      Where to store the Json String
*  @param[out]    JsonStringSize  Size of Json String
*
*  @retval   Status               Always returns success.
*
**/
EFI_STATUS
EFIAPI
DfciRequestJsonFromUSB (
    IN  CHAR16          *FileName,
    OUT CHAR8          **JsonString,
    OUT UINTN           *JsonStringSize
  ) {

    CHAR8      *Buffer;
    UINTN       BufferSize;
    EFI_STATUS  Status;

    if ((FileName == NULL) ||
        (JsonString == NULL) ||
        (JsonStringSize == NULL)) {
        DEBUG((DEBUG_ERROR, "Filename, JsonString, or JsonStringSize is NULL\n"));
        return EFI_INVALID_PARAMETER;
    }

    Status = FindUsbDriveWithDfciUpdate (FileName, &Buffer, &BufferSize);
    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR, "Unable to read update. Code=%r\n", Status));
    } else {
        *JsonString = Buffer;
        *JsonStringSize = BufferSize;
    }

    return Status;
}
