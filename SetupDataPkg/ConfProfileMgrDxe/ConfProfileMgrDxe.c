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
  )
{
  EFI_STATUS             Status;
  CONFIG_VAR_LIST_ENTRY  *VarList     = NULL;
  UINTN                  VarListCount = 0;
  UINT32                 i;
  BOOLEAN                ValidationFailure = FALSE;
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
    if ((Status != EFI_BUFFER_TOO_SMALL) && EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a failed to read variable %s Status: (%r)!\n", __FUNCTION__, VarList[i].Name, Status));
      ASSERT (FALSE);
      ValidationFailure = TRUE;
      goto Done;
    }

    if ((Size != VarList[i].DataSize) ||
        (0 != CompareMem (Data, VarList[i].Data, Size)) ||
        (Attributes != VarList[i].Attributes))
    {
      DEBUG ((DEBUG_ERROR, "%a active profile does not match variable store %s!\n", __FUNCTION__, VarList[i].Name));
      ValidationFailure = TRUE;
      goto Done;
    }

    FreePool (Data);
    Data = NULL;
  }

Done:
  if (Data != NULL) {
    FreePool (Data);
  }

  if (ValidationFailure) {
    // Write profile values and reset the system
    for (i = 0; i < VarListCount; i++) {
      Status = gRT->SetVariable (
                      VarList[i].Name,
                      &VarList[i].Guid,
                      VarList[i].Attributes,
                      VarList[i].DataSize,
                      VarList[i].Data
                      );

      // We should not fail to write any of these variables, but if so, try to write the rest
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a failed to write variable %s Status: (%r)!\n", __FUNCTION__, VarList[i].Name, Status));
        ASSERT (FALSE);
      }
    }

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
  EFI_GUID    *ActiveProfileGuid = NULL;
  EFI_STATUS  Status;
  UINTN       Size = sizeof (EFI_GUID);
  UINT32      Attributes;
  EFI_GUID    CachedProfile;
  BOOLEAN     NeedToFreeMem = TRUE;

  // RetrieveActiveProfileGuid will validate the profile guid is valid before returning
  Status = RetrieveActiveProfileGuid (&ActiveProfileGuid);

  // Check if the status failed, but don't error out, try to use cached profile
  if (EFI_ERROR (Status) || (ActiveProfileGuid == NULL)) {
    NeedToFreeMem = FALSE;
    DEBUG ((DEBUG_ERROR, "%a RetrieveActiveProfileGuid failed (%r)! Attempting to set cached profile\n", __FUNCTION__, Status));

    Status = gRT->GetVariable (
                    CACHED_CONF_PROFILE_VARIABLE_NAME,
                    &gConfProfileMgrVariableGuid,
                    &Attributes,
                    &Size,
                    &CachedProfile
                    );

    if (EFI_ERROR (Status) || (Size != sizeof (EFI_GUID))) {
      DEBUG ((DEBUG_ERROR, "%a failed to read cached profile, defaulting to default profile (%r)!\n", __FUNCTION__, Status));
      ActiveProfileGuid = (EFI_GUID *)&gSetupDataPkgGenericProfileGuid;
    } else {
      ActiveProfileGuid = &CachedProfile;
    }
  }

  // Set the PCD for the profile parser lib to read during profile validation and for the ConfApp
  Status = PcdSetPtrS (PcdSetupConfigActiveProfileFile, &Size, ActiveProfileGuid);

  // If we fail to set the PCD, the system will still be in the correct profile, but the ConfApp will report that
  // the generic profile is active, so we still want to validate the active profile
  if (EFI_ERROR (Status) || (Size != sizeof (EFI_GUID))) {
    DEBUG ((DEBUG_ERROR, "%a failed to set ActiveProfile PCD (%r)!\n", __FUNCTION__, Status));
    ASSERT (FALSE);
  }

  // We have the active profile guid, we need to load it and validate the contents against flash.
  // ValidateProfile does not return a status, in case of failure, it writes the chosen profile to flash
  // and resets the system. Only validate the profile if we are in CUSTOMER_MODE (not manufacturing mode),
  // as in debug and bringup scenarios the profile may be expected to not match
  if (!IsSystemInManufacturingMode ()) {
    ValidateActiveProfile ();
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

  // Set the cached variable, in case of communication failure in the future
  Status = gRT->SetVariable (
                  CACHED_CONF_PROFILE_VARIABLE_NAME,
                  &gConfProfileMgrVariableGuid,
                  Attributes,
                  Size,
                  ActiveProfileGuid
                  );

  // If we fail to cache the profile GUID, don't fail, system may just default to the generic profile in the future
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a failed to write cached profile variable Status: (%r)!\n", __FUNCTION__, Status));
  }

  if (NeedToFreeMem && ActiveProfileGuid) {
    FreePool (ActiveProfileGuid);
  }

  return Status;
}
