/** @file
  Library interface to query the active configuration profile.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/ActiveProfileSelectorLib.h>

/**
  Return which profile is the active profile for this boot. For the null lib, just return the PCD defined value.

  @param[out] ActiveProfileGuid   The file GUID for the active profile. Caller allocates and frees memory.
                                  The Generic Profile file Guid is returned in case of failure, aside from
                                  EFI_INVALID_PARAMETER.

  @retval EFI_INVALID_PARAMETER   Input argument is null.
  @retval EFI_COMPROMISED_DATA    The source of truth for profile selection has returned a garbage value,
                                  default profile returned.
  @retval EFI_SUCCESS             The operation succeeds.

**/
EFI_STATUS
EFIAPI
RetrieveActiveProfileGuid (
  OUT EFI_GUID  **ActiveProfileGuid
  )
{
  if (ActiveProfileGuid == NULL) {
    DEBUG ((DEBUG_ERROR, "%a Null parameter passed\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  *ActiveProfileGuid = (EFI_GUID *)PcdGetPtr (PcdSetupConfigActiveProfileFile);

  return EFI_SUCCESS;
}
