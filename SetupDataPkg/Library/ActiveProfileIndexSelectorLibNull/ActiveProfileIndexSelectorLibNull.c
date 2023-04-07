/** @file ActiveProfileIndexSelectorLibNull.c
  Null instance of ActiveProfileIndexSelectorLib. It is expected that the OEM/Platform
  will override this library to query the current boot active profile index from the
  proper source of truth.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Uefi.h>
#include <Library/BaseLib.h>

/**
  Return which profile is the active profile for this boot.
  This function validates the profile GUID is valid.

  @param[out] ActiveProfileIndex  The index for the active profile. A value of -1, when combined with a return
                                  value of EFI_SUCCESS, indicates that the default profile has been chosen. If the
                                  return value is not EFI_SUCCESS, the value of ActiveProfileIndex shall not be updated.

  @retval EFI_INVALID_PARAMETER   Input argument is null.
  @retval EFI_NO_RESPONSE         The source of truth for profile selection has returned a garbage value or not replied.
  @retval EFI_SUCCESS             The operation succeeds and ActiveProfileIndex contains the valid active profile
                                  index for this boot.
**/
EFI_STATUS
EFIAPI
GetActiveProfileIndex (
  OUT INT32  *ActiveProfileIndex
  )
{
  if (ActiveProfileIndex == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Null instance, we just return the default profile
  *ActiveProfileIndex = -1;

  return EFI_SUCCESS;
}
