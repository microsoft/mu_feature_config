/** @file
  Driver to query active configuration profile guid and validate flash contents, writing profile values and resetting
  the system as necessary.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/ResetUtilityLib.h>
#include <Library/ConfigVariableListLib.h>
#include <Library/ConfigSystemModeLib.h>
#include <Library/ActiveProfileSelectorLib.h>

#define CACHED_CONF_PROFILE_VARIABLE_NAME  L"CachedConfProfileGuid"

/**
  Validate the chosen profile values against variable storage. If any value mismatch, write profile values to flash
  and reset the system.

  @param[in] ActiveProfileGuid  File guid for the currently active profile
**/
STATIC
VOID
ValidateActiveProfile (
  VOID
  )
{
  EFI_STATUS             Status;
  CONFIG_VAR_LIST_ENTRY  *VarList     = NULL;
  UINTN                  VarListCount = 0;
  UINT32                 i;
  BOOLEAN                ValidationFailure = FALSE;
  BOOLEAN                VariableInvalid   = FALSE;
  UINTN                  Size;
  UINT32                 Attributes;
  VOID                   *Data = NULL;

  Status = RetrieveActiveConfigVarList (&VarList, &VarListCount);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a - Retrieving Variable List failed - %r\n", __FUNCTION__, Status));
    ASSERT (FALSE);
    goto Done;
  }

  if ((VarList == NULL) || (VarListCount == 0)) {
    // This should not be the case
    DEBUG ((DEBUG_ERROR, "%a Retrieved config data is NULL.\n", __FUNCTION__));
    ASSERT (FALSE);
    goto Done;
  }

  for (i = 0; i < VarListCount; i++) {
    Data = AllocatePool (VarList[i].DataSize);

    if (Data == NULL) {
      DEBUG ((DEBUG_ERROR, "%a Failed to allocate memory, size: %u var: %s.\n", __FUNCTION__, VarList[i].DataSize, VarList[i].Name));
      ASSERT (FALSE);
      continue;
    }

    Size = VarList[i].DataSize;

    // Compare profile value against flash value
    Status = gRT->GetVariable (
                    VarList[i].Name,
                    &VarList[i].Guid,
                    &Attributes,
                    &Size,
                    Data
                    );

    // We should not fail to read any of these variables, if so, write variables, reset
    // It is possible that we will fail to read the variable due to the size being different, indicating
    // the variable has changed. That may be expected and should be handled as the case of the active profile
    // not matching the variable store
    if (EFI_ERROR (Status) ||
        (Size != VarList[i].DataSize) ||
        (Attributes != VarList[i].Attributes))
    {
      DEBUG ((DEBUG_ERROR, "%a variable %s does not match profile, deleting!\n", __FUNCTION__, VarList[i].Name));
      ValidationFailure = TRUE;
      VariableInvalid   = TRUE;

      // DataSize or Attributes incorrect. Try deleting the variable so we can set the correct values
      Status = gRT->SetVariable (
                      VarList[i].Name,
                      &VarList[i].Guid,
                      0,
                      0,
                      NULL
                      );

      // We should not fail to delete any of these variables, but if so, try to delete the rest
      if ((Status != EFI_NOT_FOUND) && EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a failed to delete variable %s Status: (%r)!\n", __FUNCTION__, VarList[i].Name, Status));
        ASSERT (FALSE);
      }
    }

    if (VariableInvalid || (0 != CompareMem (Data, VarList[i].Data, VarList[i].DataSize))) {
      DEBUG ((DEBUG_ERROR, "%a variable %s does not match profile, overwriting!\n", __FUNCTION__, VarList[i].Name));
      // either the variable was previously deleted and needs to be written or the Attributes and DataSize were fine
      // but the Data does not match, so it can be rewritten without being deleted
      Status = gRT->SetVariable (
                      VarList[i].Name,
                      &VarList[i].Guid,
                      VarList[i].Attributes,
                      VarList[i].DataSize,
                      VarList[i].Data
                      );

      ValidationFailure = TRUE;

      // We should not fail to write any of these variables, but if so, try to write the rest
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a failed to write variable %s Status: (%r)!\n", __FUNCTION__, VarList[i].Name, Status));
        ASSERT (FALSE);
      }
    }

    FreePool (Data);
    Data            = NULL;
    VariableInvalid = FALSE;
  }

Done:
  if (Data != NULL) {
    FreePool (Data);
  }

  if (ValidationFailure) {
    // We've updated the variables, now reset the system
    DEBUG ((DEBUG_ERROR, "%a profile written, resetting system\n", __FUNCTION__));
    ResetSystemWithSubtype (EfiResetCold, &gConfProfileMgrResetGuid);
    // Reset system should not return, dead loop if it does
    CpuDeadLoop ();
  }

  if (VarList != NULL) {
    for (i = 0; i < VarListCount; i++) {
      // Also free the data and name
      if (VarList[i].Name != NULL) {
        FreePool (VarList[i].Name);
      }

      if (VarList[i].Data != NULL) {
        FreePool (VarList[i].Data);
      }
    }

    FreePool (VarList);
  }

  return;
}

/**
  This driver will query the active configuration profile and validate the flash contents match. If not, the
  profile contents will be written to flash and the system reset.

  @param  ImageHandle  Handle to this image.
  @param  SystemTable  Pointer to the system table

  @retval  EFI_SUCCESS   The driver validated the profile against flash contents successfully.
  @retval  !EFI_SUCCESS  The driver failed to validate the profile against flash contents successfully.
**/
EFI_STATUS
EFIAPI
ConfProfileMgrDxeEntry (
  IN  EFI_HANDLE        ImageHandle,
  IN  EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_GUID    ActiveProfileGuid;
  EFI_STATUS  Status;
  EFI_GUID    CachedProfile;
  UINT32      i;
  UINT32      NumProfiles        = 0;
  EFI_GUID    *ValidGuids        = NULL;
  UINTN       GuidsSize          = 0;
  UINTN       Size               = sizeof (EFI_GUID);
  UINT32      Attributes         = EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS;
  BOOLEAN     FoundProfileInList = FALSE;
  BOOLEAN     FoundCachedProfile = FALSE;

  // read the cached profile value. This will be used in case RetrieveActiveProfileGuid fails
  // and to check if we need to write the variable if the cached value is different than the new value
  Status = gRT->GetVariable (
                  CACHED_CONF_PROFILE_VARIABLE_NAME,
                  &gConfProfileMgrVariableGuid,
                  &Attributes,
                  &Size,
                  &CachedProfile
                  );

  if (EFI_ERROR (Status) || (Size != sizeof (EFI_GUID))) {
    // failed to read the cached profile, fail back to generic profile if required
    DEBUG ((DEBUG_WARN, "%a failed to read cached profile, expected on first boot (%r)!\n", __FUNCTION__, Status));
    Size = sizeof (EFI_GUID);
    CopyMem (&CachedProfile, (EFI_GUID *)&gSetupDataPkgGenericProfileGuid, Size);
  } else {
    // used to tell if we need to write the cached profile later or not
    FoundCachedProfile = TRUE;
  }

  // RetrieveActiveProfileGuid will validate the profile guid is valid before returning
  Status = RetrieveActiveProfileGuid (&ActiveProfileGuid);

  // Check if the status failed, but don't error out, try to use cached profile
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a RetrieveActiveProfileGuid failed (%r)! Attempting to use cached profile\n", __FUNCTION__, Status));

    if (!FoundCachedProfile) {
      DEBUG ((DEBUG_ERROR, "%a Failed to retrieve cached profile, using generic profile\n", __FUNCTION__));
    }

    CopyMem (&ActiveProfileGuid, &CachedProfile, Size);
  }

  // Validate this profile is in the list of profiles for this platform
  NumProfiles = PcdGetSize (PcdConfigurationProfileList);

  // if we get a bad size for the profile list, we need to fail to the default profile
  // as we cannot validate the received profile
  if ((NumProfiles == 0) || (NumProfiles % Size != 0)) {
    DEBUG ((DEBUG_ERROR, "%a Invalid number of bytes in PcdConfigurationProfileList: %u, using generic profile\n", __FUNCTION__, NumProfiles));
    ASSERT (FALSE);
    CopyMem (&ActiveProfileGuid, (EFI_GUID *)&gSetupDataPkgGenericProfileGuid, Size);
  } else {
    NumProfiles /= sizeof (EFI_GUID);

    ValidGuids = (EFI_GUID *)PcdGetPtr (PcdConfigurationProfileList);
    GuidsSize  = PcdGetSize (PcdConfigurationProfileList);

    // if we can't find the list of valid profile guids or if the list is not 16 bit aligned,
    // we need to fail back to the default profile
    if ((ValidGuids == NULL) || ((GuidsSize % sizeof (EFI_GUID)) != 0)) {
      DEBUG ((DEBUG_ERROR, "%a Failed to get list of valid GUIDs, using generic profile\n", __FUNCTION__));
      ASSERT (FALSE);
      CopyMem (&ActiveProfileGuid, (EFI_GUID *)&gSetupDataPkgGenericProfileGuid, Size);
    } else {
      // validate that the returned profile guid is one of the known profile guids
      for (i = 0; i < NumProfiles; i++) {
        if (0 == CompareMem (&ActiveProfileGuid, &(ValidGuids[i]), Size)) {
          // we found the profile we are in
          FoundProfileInList = TRUE;
          break;
        }
      }

      // if we didn't find the profile and it is not the generic profile, it is invalid and
      // we default to the generic profile
      if ((!FoundProfileInList) &&
          ((0 != CompareMem (&ActiveProfileGuid, (EFI_GUID *)&gSetupDataPkgGenericProfileGuid, Size))))
      {
        DEBUG ((DEBUG_ERROR, "%a Invalid profile GUID received, defaulting to default profile\n", __FUNCTION__));
        CopyMem (&ActiveProfileGuid, (EFI_GUID *)&gSetupDataPkgGenericProfileGuid, Size);
      }
    }
  }

  // Set the PCD for the profile parser lib to read during profile validation and for the ConfApp
  Status = PcdSetPtrS (PcdSetupConfigActiveProfileFile, &Size, &ActiveProfileGuid);

  // If we fail to set the PCD, the system will still be in the correct profile, but the ConfApp will report that
  // the generic profile is active, so we still want to validate the active profile
  if (EFI_ERROR (Status) || (Size != sizeof (EFI_GUID))) {
    DEBUG ((DEBUG_ERROR, "%a failed to set ActiveProfile PCD (%r)!\n", __FUNCTION__, Status));
    ASSERT (FALSE);
    Size = sizeof (EFI_GUID);
  }

  // to avoid perf hit, only write var in case of mismatch with cached variable
  // write the cached profile before we validate against the current variable store contents
  // so that in case of mismatch we have the cached variable stored for next boot
  if ((!FoundCachedProfile) || (0 != CompareMem (&CachedProfile, &ActiveProfileGuid, Size))) {
    Status = gRT->SetVariable (
                    CACHED_CONF_PROFILE_VARIABLE_NAME,
                    &gConfProfileMgrVariableGuid,
                    Attributes,
                    Size,
                    &ActiveProfileGuid
                    );

    // If we fail to cache the profile GUID, don't fail, system may just default to the generic profile in the future
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "%a failed to write cached profile variable Status: (%r)!\n", __FUNCTION__, Status));
    }
  }

  // We have the active profile guid, we need to load it and validate the contents against flash.
  // ValidateProfile does not return a status, in case of failure, it writes the chosen profile to flash
  // and resets the system. Only validate the profile if we are in CUSTOMER_MODE (not manufacturing mode),
  // as in debug and bringup scenarios the profile may be expected to not match
  if (!IsSystemInManufacturingMode ()) {
    DEBUG ((DEBUG_INFO, "%a System not in MFG Mode, validating profile matches variable storage\n", __FUNCTION__));
    ValidateActiveProfile ();
  } else {
    DEBUG ((DEBUG_INFO, "%a System in MFG Mode, not validating profile matches variable storage\n", __FUNCTION__));
  }

  // Publish protocol for the configuration settings provider to be able to load with the correct profile in the PCD
  Status = gBS->InstallProtocolInterface (
                  &ImageHandle,
                  (EFI_GUID *)&gConfProfileMgrProfileValidProtocolGuid,
                  EFI_NATIVE_INTERFACE,
                  NULL
                  );

  // if we fail to publish the protocol, the settings provider will not come up
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a failed to publish protocol (%r)!\n", __FUNCTION__, Status));
    ASSERT (FALSE);
  }

  return Status;
}
