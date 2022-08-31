/** @file
  Settings provider driver to register configuration data setter and getters.

  Copyright (c) 2017 - 2020, Intel Corporation. All rights reserved.<BR>
  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiPei.h>

#include <Guid/ZeroGuid.h>

#include <Library/PcdLib.h>
#include <Library/ConfigSystemModeLib.h>

/**
  This function initializes the DFCI unsigned XML PCD based on the system
  operation mode.

  @param  FileHandle  Handle of the file being invoked.
  @param  PeiServices Describes the list of possible PEI Services.

  @retval  EFI_SUCCESS   The PEIM executed normally.
  @retval  !EFI_SUCCESS  The PEIM failed to execute normally.
**/
EFI_STATUS
EFIAPI
SgiPlatformPeim (
 IN       EFI_PEI_FILE_HANDLE  FileHandle,
 IN CONST EFI_PEI_SERVICES     **PeiServices
  )
{
  if (!IsSystemInManufacturingMode ()) {
    // Invalidate the PCD if system operation does not allow it.
    PcdSetPtrS (PcdUnsignedPermissionsFile, sizeof (EFI_GUID), &gZeroGuid);
  }

  return EFI_SUCCESS;
}