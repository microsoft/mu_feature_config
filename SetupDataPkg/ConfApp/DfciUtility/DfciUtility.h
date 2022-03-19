/** @file
DfciUtility.h

DfciUtility contains useful functions

Copyright (C) Microsoft Corporation. All rights reserved.
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __DFCI_UTILITY_H__
#define __DFCI_UTILITY_H__

#include <Uefi.h>

#include <DfciSystemSettingTypes.h>

#define DFCI_MAX_STRING_LEN  (1024)

typedef struct {
  CHAR8    *SerialNumber;
  UINTN    SerialNumberSize;
  CHAR8    *Manufacturer;
  UINTN    ManufacturerSize;
  CHAR8    *ProductName;
  UINTN    ProductNameSize;
} DFCI_SYSTEM_INFORMATION;

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
  IN DFCI_SYSTEM_INFORMATION  *DfciInfo
  );

/**
 * DfciFreeInfo
 *
 * @param[in]  DfciInfo
 *
 * Free the system identifier elements
 **/
VOID
DfciFreeSystemInfo (
  IN DFCI_SYSTEM_INFORMATION  *DfciInfo
  );

#endif
