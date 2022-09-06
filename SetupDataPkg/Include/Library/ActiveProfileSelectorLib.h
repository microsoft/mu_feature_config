/** @file
  Library interface to select active profile for this boot.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __ACTIVE_PROFILE_SELECTOR_LIB_H__
#define __ACTIVE_PROFILE_SELECTOR_LIB_H__

/**
  Return which profile is the active profile for this boot.
  This function validates the profile GUID is valid.

  @param[out] ActiveProfileGuid   The file GUID for the active profile. This function allocates mem, caller frees mem.
                                  NULL is returned in case of failure.

  @retval EFI_INVALID_PARAMETER   Input argument is null.
  @retval EFI_NO_RESPONSE         The source of truth for profile selection has returned a garbage value or not replied.
  @retval EFI_OUT_OF_RESOURCES    Memory allocation failed.
  @retval EFI_SUCCESS             The operation succeeds.

**/
EFI_STATUS
EFIAPI
RetrieveActiveProfileGuid (
  OUT EFI_GUID  **ActiveProfileGuid
  );

#endif // __ACTIVE_PROFILE_SELECTOR_LIB_H__
