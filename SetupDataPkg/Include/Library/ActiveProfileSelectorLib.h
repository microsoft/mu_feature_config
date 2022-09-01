/** @file
  Library interface to select active profile for this boot.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __ACTIVE_PROFILE_SELECTOR_LIB_H__
#define __ACTIVE_PROFILE_SELECTOR_LIB_H__

/**
  Return which profile is the active profile for this boot.

  @param[out] ActiveProfileGuid   The file GUID for the active profile. Caller allocates and frees memory.
                                  The Generic Profile file Guid is returned in case of failure.

  @retval EFI_INVALID_PARAMETER   Input argument is null.
  @retval EFI_COMPROMISED_DATA    The source of truth for profile selection has returned a garbage value,
                                  default profile returned.
  @retval EFI_SUCCESS             The operation succeeds.

**/
EFI_STATUS
EFIAPI
RetrieveActiveProfileGuid (
  OUT EFI_GUID **ActiveProfileGuid
  );

#endif // __ACTIVE_PROFILE_SELECTOR_LIB_H__