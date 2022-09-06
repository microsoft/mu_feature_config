/** @file
  Library interface to query the active configuration profile.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/ActiveProfileSelectorLib.h>

/**
  Return which profile is the active profile for this boot.
  This function validates the profile GUID is valid.

  @param[out] ActiveProfileGuid   The file GUID for the active profile. Caller frees memory.
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
  )
{
  EFI_GUID  *ActiveProfile = NULL;
  UINTN     Size           = sizeof (EFI_GUID);
  UINT32    NumProfiles    = 0;
  UINT32    i;
  EFI_GUID  *ValidGuids = NULL;

  if (ActiveProfileGuid == NULL) {
    DEBUG ((DEBUG_ERROR, "%a Null parameter passed\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  ActiveProfile = AllocatePool (Size);

  if (ActiveProfile == NULL) {
    DEBUG ((DEBUG_ERROR, "%a Failed to allocate memory size: %u\n", __FUNCTION__, Size));
    return EFI_OUT_OF_RESOURCES;
  }

  CopyMem (ActiveProfile, PcdGetPtr (PcdSetupConfigActiveProfileFile), Size);

  NumProfiles = PcdGet32 (PcdConfigurationProfileCount);

  if (NumProfiles == 0) {
    DEBUG ((DEBUG_ERROR, "%a Failed to get NumProfiles\n", __FUNCTION__));
    return EFI_NO_RESPONSE;
  }

  ValidGuids = (EFI_GUID *)PcdGetPtr (PcdConfigurationProfileList);

  if (ValidGuids == NULL) {
    DEBUG ((DEBUG_ERROR, "%a Failed to get list of valid GUIDs\n", __FUNCTION__));
    return EFI_NO_RESPONSE;
  }

  // validate that the returned profile guid is one of the known profile guids
  for (i = 0; i < NumProfiles; i++) {
    if (0 == CompareMem (ActiveProfile, &ValidGuids[i], sizeof (*ActiveProfile))) {
      // we found the profile we are in
      *ActiveProfileGuid = ActiveProfile;

      return EFI_SUCCESS;
    }
  }

  return EFI_NO_RESPONSE;
}
