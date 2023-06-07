/** @file ActiveProfileIndexSelectorLib.h

  Library interface to select the active profile index for this boot.
  The OEM/Platform must override this library to choose the correct
  method for ascertaining the profile index for a given boot.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef ACTIVE_PROFILE_INDEX_SELECTOR_LIB_H_
#define ACTIVE_PROFILE_INDEX_SELECTOR_LIB_H_

// macro to represent the generic profile, used as a default if no profile is selected or a failure occurs
#define GENERIC_PROFILE_INDEX MAX_UINT32

/**
  Return which profile is the active profile for this boot.
  This function validates the profile GUID is valid.

  @param[out] ActiveProfileIndex  The index for the active profile. A value of MAX_UINT32, when combined with a return
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
  OUT UINT32  *ActiveProfileIndex
  );

#endif // ACTIVE_PROFILE_INDEX_SELECTOR_LIB_H_
