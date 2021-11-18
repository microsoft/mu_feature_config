/** @file
DfciUtility.c

This module will request new DFCI configuration data from server.

Copyright (C) Microsoft Corporation. All rights reserved.
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>



#include <DfciSystemSettingTypes.h>

#include <Protocol/DfciSettingAccess.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DfciDeviceIdSupportLib.h>
#include <Library/HiiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include "DfciUtility.h"

/**
 * DfciFreeInfo
 *
 * @param[in]  DfciInfo
 *
 * Free the system identifier elements
 **/
VOID
DfciFreeSystemInfo (
    IN DFCI_SYSTEM_INFORMATION *DfciInfo
  ) {

    if (NULL != DfciInfo->SerialNumber) {
        FreePool (DfciInfo->SerialNumber);
    }

    if (NULL != DfciInfo->Manufacturer) {
        FreePool (DfciInfo->Manufacturer);
    }

    if (NULL != DfciInfo->ProductName) {
        FreePool (DfciInfo->ProductName);
    }
}

/**
 * DfciGetSystemInfo
 *
 * Get the system identifier elements
 *
 * @param[in] DfciInfo
 *
 **/
EFI_STATUS
DfciGetSystemInfo (
    IN DFCI_SYSTEM_INFORMATION *DfciInfo
  ) {

    EFI_STATUS      Status;

    ZeroMem (DfciInfo, sizeof(DFCI_SYSTEM_INFORMATION));

    Status = DfciIdSupportGetSerialNumber (&DfciInfo->SerialNumber, &DfciInfo->SerialNumberSize);
    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR, "Unable to get ProductName. Code=%r\n", Status));
        goto Error;
    }

    Status = DfciIdSupportGetManufacturer (&DfciInfo->Manufacturer, &DfciInfo->ManufacturerSize);
    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR, "Unable to get ProductName. Code=%r\n", Status));
        goto Error;
    }

    Status = DfciIdSupportGetProductName (&DfciInfo->ProductName, &DfciInfo->ProductNameSize);
    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_ERROR, "Unable to get ProductName. Code=%r\n", Status));
        goto Error;
    }

    return EFI_SUCCESS;

Error:
    DfciFreeSystemInfo (DfciInfo);
    return Status;
}
