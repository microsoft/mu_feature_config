/** @file
  Settings provider driver to register configuration data setter and getters.

  Copyright (c) 2017 - 2020, Intel Corporation. All rights reserved.<BR>
  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <DfciSystemSettingTypes.h>
#include <Guid/MuVarPolicyFoundationDxe.h>
#include <Pi/PiFirmwareFile.h>

#include <Protocol/DfciSettingsProvider.h>
#include <Protocol/VariablePolicy.h>

#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiLib.h>
#include <Library/DxeServicesLib.h>
#include <Library/ConfigDataLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/ConfigBlobBaseLib.h>
#include <Library/VariablePolicyHelperLib.h>
#include <Library/SafeIntLib.h>
#include <Library/ConfigVariableListLib.h>

DFCI_SETTING_PROVIDER_SUPPORT_PROTOCOL  *mSettingProviderProtocol = NULL;
EDKII_VARIABLE_POLICY_PROTOCOL          *mVariablePolicy          = NULL;

/**
  Set configuration to default value from UEFI FV.

  @param This          Setting Provider protocol

  @retval EFI_SUCCESS  default set
  @retval ERROR        Error
**/
EFI_STATUS
EFIAPI
SingleConfDataSetDefault (
  IN  CONST DFCI_SETTING_PROVIDER  *This
  )
{
  EFI_STATUS             Status;
  CONFIG_VAR_LIST_ENTRY  VarListEntry;

  ZeroMem (&VarListEntry, sizeof (VarListEntry));

  if ((This == NULL) || (This->Id == NULL)) {
    Status = EFI_INVALID_PARAMETER;
    goto Done;
  }

  Status = QuerySingleActiveConfigAsciiVarList (This->Id, &VarListEntry);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  Status = gRT->SetVariable (
                  VarListEntry.Name,
                  &VarListEntry.Guid,
                  VarListEntry.Attributes,
                  VarListEntry.DataSize,
                  VarListEntry.Data
                  );

Done:
  if (VarListEntry.Name != NULL) {
    FreePool (VarListEntry.Name);
  }

  if (VarListEntry.Data != NULL) {
    FreePool (VarListEntry.Data);
  }

  return Status;
}

/**
  Get the default value of a single setting from UEFI FV.
  This getter will serialize default configuration setting
  to printable strings to be used in Config App.

  @param This           Setting Provider
  @param ValueSize      IN=Size of location to store value
                        OUT=Size of value stored
  @param Value          Output parameter for the settings default value.
                        The type and size is based on the provider type
                        and must be allocated by the caller.

  @retval EFI_SUCCESS           If the default could be returned.
  @retval EFI_BUFFER_TOO_SMALL  If the ValueSize on input is too small
  @retval ERROR                 Error
**/
EFI_STATUS
EFIAPI
SingleConfDataGetDefault (
  IN  CONST DFCI_SETTING_PROVIDER  *This,
  IN  OUT   UINTN                  *ValueSize,
  OUT       UINT8                  *Value
  )
{
  EFI_STATUS             Status;
  CONFIG_VAR_LIST_ENTRY  VarListEntry;
  UINTN                  NeededSize;
  RUNTIME_VAR_LIST_HDR   VarListHdr;
  UINTN                  Offset;

  ZeroMem (&VarListEntry, sizeof (CONFIG_VAR_LIST_ENTRY));

  if ((This == NULL) || (This->Id == NULL) || (ValueSize == NULL) || ((*ValueSize != 0) && (Value == NULL))) {
    Status = EFI_INVALID_PARAMETER;
    goto Done;
  }

  Status = QuerySingleActiveConfigAsciiVarList (This->Id, &VarListEntry);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  /*
    * Var List is in DmpStore format:
    *
    *  struct {
    *    RUNTIME_VAR_LIST_HDR VarList;
    *    CHAR16 Name[VarList->NameSize/2];
    *    EFI_GUID Guid;
    *    UINT32 Attributes;
    *    CHAR8 Data[VarList->DataSize];
    *    UINT32 CRC32; // CRC32 of all bytes from VarList to end of Data
    *  }
    */

  VarListHdr.NameSize = (UINT32)(StrnLenS (VarListEntry.Name, DFCI_MAX_ID_LEN) + 1);
  VarListHdr.DataSize = VarListEntry.DataSize;

  NeededSize = sizeof (VarListHdr) + VarListHdr.NameSize + VarListHdr.DataSize + sizeof (VarListEntry.Guid) +
               sizeof (VarListEntry.Attributes) + sizeof (UINT32);

  if (NeededSize > *ValueSize) {
    *ValueSize = NeededSize;
    Status     = EFI_BUFFER_TOO_SMALL;
    goto Done;
  }

  Offset = 0;
  // Header
  CopyMem (Value, &VarListHdr, sizeof (VarListHdr));
  Offset += sizeof (VarListHdr);

  // Name
  CopyMem (Value + Offset, VarListEntry.Name, VarListHdr.NameSize);
  Offset += VarListHdr.NameSize;

  // Guid
  CopyMem (Value + Offset, &VarListEntry.Guid, sizeof (VarListEntry.Guid));
  Offset += sizeof (VarListEntry.Guid);

  // Attributes
  CopyMem (Value + Offset, &VarListEntry.Attributes, sizeof (VarListEntry.Attributes));
  Offset += sizeof (VarListEntry.Attributes);

  // Data
  CopyMem (Value + Offset, VarListEntry.Data, sizeof (VarListEntry.DataSize));
  Offset += VarListHdr.DataSize;

  // CRC32
  *(UINT32 *)(Value + Offset) = CalculateCrc32 (Value, Offset);
  Offset                     += sizeof (UINT32);

  // They should still match in size...
  ASSERT (Offset == NeededSize);

Done:
  if (VarListEntry.Name != NULL) {
    FreePool (VarListEntry.Name);
  }

  if (VarListEntry.Data != NULL) {
    FreePool (VarListEntry.Data);
  }

  return Status;
}

/**
  Set new configuration value to variable storage.

  @param This      Provider Setting
  @param Value     a pointer to a datatype defined by the Type for this setting.
  @param ValueSize Size of the data for this setting.
  @param Flags     Informational Flags passed to the SET and/or Returned as a result of the set

  @retval EFI_SUCCESS If setting could be set.  Check flags for other info (reset required, etc)
  @retval Error       Setting not set.
**/
EFI_STATUS
EFIAPI
SingleConfDataSet (
  IN  CONST DFCI_SETTING_PROVIDER  *This,
  IN        UINTN                  ValueSize,
  IN  CONST UINT8                  *Value,
  OUT DFCI_SETTING_FLAGS           *Flags
  )
{
  EFI_STATUS             Status;
  CONFIG_VAR_LIST_ENTRY  VarListEntry;

  if ((This == NULL) || (This->Id == NULL) || (Value == NULL) || (ValueSize == 0) || (Flags == NULL)) {
    Status = EFI_INVALID_PARAMETER;
    goto Done;
  }

  Status = QuerySingleActiveConfigAsciiVarList (This->Id, &VarListEntry);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  // Now set it to non-volatile variable
  Status = gRT->SetVariable (VarListEntry.Name, &VarListEntry.Guid, VarListEntry.Attributes, ValueSize, (VOID *)Value);

  if (!EFI_ERROR (Status)) {
    *Flags |= DFCI_SETTING_FLAGS_OUT_REBOOT_REQUIRED;
  }

Done:
  if (VarListEntry.Name != NULL) {
    FreePool (VarListEntry.Name);
  }

  if (VarListEntry.Data != NULL) {
    FreePool (VarListEntry.Data);
  }

  return Status;
}

/**
  Set new configuration value to variable storage.

  @param This      Provider Setting
  @param Value     a pointer to a datatype defined by the Type for this setting.
  @param ValueSize Size of the data for this setting.
  @param Flags     Informational Flags passed to the SET and/or Returned as a result of the set

  @retval EFI_SUCCESS If setting could be set.  Check flags for other info (reset required, etc)
  @retval Error       Setting not set.
**/
EFI_STATUS
EFIAPI
SingleConfDataGet (
  IN CONST DFCI_SETTING_PROVIDER  *This,
  IN OUT   UINTN                  *ValueSize,
  OUT      UINT8                  *Value
  )
{
  EFI_STATUS             Status;
  CONFIG_VAR_LIST_ENTRY  VarListEntry;
  UINTN                  NeededSize;
  RUNTIME_VAR_LIST_HDR   VarListHdr;
  UINTN                  Offset;
  UINTN                  DataSize;

  if ((This == NULL) || (This->Id == NULL) || (ValueSize == NULL) || ((*ValueSize != 0) && (Value == NULL))) {
    Status = EFI_INVALID_PARAMETER;
    goto Done;
  }

  Status = QuerySingleActiveConfigAsciiVarList (This->Id, &VarListEntry);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  /*
    * Var List is in DmpStore format:
    *
    *  struct {
    *    RUNTIME_VAR_LIST_HDR VarList;
    *    CHAR16 Name[VarList->NameSize/2];
    *    EFI_GUID Guid;
    *    UINT32 Attributes;
    *    CHAR8 Data[VarList->DataSize];
    *    UINT32 CRC32; // CRC32 of all bytes from VarList to end of Data
    *  }
    */

  VarListHdr.NameSize = (UINT32)(StrnLenS (VarListEntry.Name, DFCI_MAX_ID_LEN) + 1);

  DataSize = 0;
  Status   = gRT->GetVariable (VarListEntry.Name, &VarListEntry.Guid, NULL, &DataSize, NULL);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    DEBUG ((DEBUG_ERROR, "Get variable call returned unexpected result %r!", Status));
    Status = EFI_ACCESS_DENIED;
    goto Done;
  }

  Status = SafeUintnToUint32 (DataSize, &VarListHdr.DataSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Get variable returned variable size too large (%lx) - %r!", DataSize, Status));
    Status = EFI_ACCESS_DENIED;
    goto Done;
  }

  NeededSize = sizeof (VarListHdr) + VarListHdr.NameSize + VarListHdr.DataSize + sizeof (VarListEntry.Guid) +
               sizeof (VarListEntry.Attributes) + sizeof (UINT32);

  if (NeededSize > *ValueSize) {
    *ValueSize = NeededSize;
    Status     = EFI_BUFFER_TOO_SMALL;
    goto Done;
  }

  Offset = 0;
  // Header
  CopyMem (Value, &VarListHdr, sizeof (VarListHdr));
  Offset += sizeof (VarListHdr);

  // Name
  CopyMem (Value + Offset, VarListEntry.Name, VarListHdr.NameSize);
  Offset += VarListHdr.NameSize;

  // Guid
  CopyMem (Value + Offset, &VarListEntry.Guid, sizeof (VarListEntry.Guid));
  Offset += sizeof (VarListEntry.Guid);

  // Attributes
  CopyMem (Value + Offset, &VarListEntry.Attributes, sizeof (VarListEntry.Attributes));
  Offset += sizeof (VarListEntry.Attributes);

  // Data
  Status = gRT->GetVariable (VarListEntry.Name, &VarListEntry.Guid, NULL, &DataSize, Value + Offset);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  Offset += VarListHdr.DataSize;

  // CRC32
  *(UINT32 *)(Value + Offset) = CalculateCrc32 (Value, Offset);
  Offset                     += sizeof (UINT32);

  // They should still match in size...
  ASSERT (Offset == NeededSize);

Done:
  if (VarListEntry.Name != NULL) {
    FreePool (VarListEntry.Name);
  }

  if (VarListEntry.Data != NULL) {
    FreePool (VarListEntry.Data);
  }

  return Status;
}

DFCI_SETTING_PROVIDER  SingleSettingProviderTemplate = {
  NULL,
  DFCI_SETTING_TYPE_BINARY,
  DFCI_SETTING_FLAGS_OUT_REBOOT_REQUIRED,
  (DFCI_SETTING_PROVIDER_SET)SingleConfDataSet,
  (DFCI_SETTING_PROVIDER_GET)SingleConfDataGet,
  (DFCI_SETTING_PROVIDER_GET_DEFAULT)SingleConfDataGetDefault,
  (DFCI_SETTING_PROVIDER_SET_DEFAULT)SingleConfDataSetDefault
};

// Helper functions to register
STATIC
EFI_STATUS
EFIAPI
RegisterSingleConfigVariable (
  CONFIG_VAR_LIST_ENTRY  *VarListEntry
  )
{
  EFI_STATUS             Status;
  UINTN                  Size;
  DFCI_SETTING_PROVIDER  *Setting = NULL;
  CHAR8                  *TempAsciiId = NULL;

  if ((mVariablePolicy == NULL) || (mSettingProviderProtocol == NULL)) {
    DEBUG ((DEBUG_ERROR, "Either setting access (%p) or variable policy policy (%p) is not ready!!!\n", mSettingProviderProtocol, mVariablePolicy));
    Status = EFI_NOT_READY;
    goto Exit;
  }

  if (VarListEntry == NULL) {
    DEBUG ((DEBUG_ERROR, "Incoming variable list entry is NULL!!!\n"));
    Status = EFI_INVALID_PARAMETER;
    goto Exit;
  }

  Setting = AllocateCopyPool (sizeof (DFCI_SETTING_PROVIDER), &SingleSettingProviderTemplate);
  if (Setting == NULL) {
    DEBUG ((DEBUG_ERROR, "Failed to allocate buffer for setting %s.\n", VarListEntry->Name));
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  Size        = StrnLenS (VarListEntry->Name, DFCI_MAX_ID_LEN) + 1;
  TempAsciiId = AllocatePool (Size);
  if (TempAsciiId == NULL) {
    DEBUG ((DEBUG_ERROR, "Failed to allocate buffer for ID %s.\n", VarListEntry->Name));
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  UnicodeStrToAsciiStrS (VarListEntry->Name, TempAsciiId, Size);
  Setting->Id = TempAsciiId;
  Status      = mSettingProviderProtocol->RegisterProvider (mSettingProviderProtocol, Setting);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to Register for ID %s.  Status = %r\n", VarListEntry->Name, Status));
  }

  Size   = 0;
  Status = gRT->GetVariable (VarListEntry->Name, &VarListEntry->Guid, NULL, &Size, NULL);
  if (Status == EFI_NOT_FOUND) {
    Status = gRT->SetVariable (VarListEntry->Name, &VarListEntry->Guid, CDATA_NV_VAR_ATTR, VarListEntry->DataSize, VarListEntry->Data);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Initializing variable %s failed - %r.\n", VarListEntry->Name, Status));
      goto Exit;
    }
  } else if (Status == EFI_BUFFER_TOO_SMALL) {
    // It was set in previous boot flow, so we are successful
    Status = EFI_SUCCESS;
  } else {
    DEBUG ((DEBUG_ERROR, "Unexpected result from GetVariable - %r.\n", Status));
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

  Status = RegisterVarStateVariablePolicy (
             mVariablePolicy,
             &VarListEntry->Guid,
             VarListEntry->Name,
             VarListEntry->DataSize,
             VARIABLE_POLICY_NO_MAX_SIZE,
             CDATA_NV_VAR_ATTR,
             (UINT32) ~CDATA_NV_VAR_ATTR,
             &gMuVarPolicyDxePhaseGuid,
             READY_TO_BOOT_INDICATOR_VAR_NAME,
             PHASE_INDICATOR_SET
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a - Registering Variable Policy for Target Variable %s failed - %r\n", __FUNCTION__, VarListEntry->Name, Status));
    goto Exit;
  }

Exit:
  if (EFI_ERROR (Status)) {
    if (Setting != NULL) {
      FreePool (Setting);
    }

    if (TempAsciiId != NULL) {
      FreePool (TempAsciiId);
    }
  }

  return Status;
}

/**
  Library design is such that a dependency on gDfciSettingsProviderSupportProtocolGuid
  is not desired.  So to resolve that a ProtocolNotify is used.

  This function gets triggered once on install and 2nd time when the Protocol gets installed.

  When the gDfciSettingsProviderSupportProtocolGuid protocol is available the function will
  loop thru all supported device disablement supported features (using PCD) and install the settings

  @param[in]  Event                 Event whose notification function is being invoked.
  @param[in]  Context               The pointer to the notification function's context, which is
                                    implementation-dependent.
**/
VOID
EFIAPI
SettingsProviderSupportProtocolNotify (
  IN  EFI_EVENT  Event,
  IN  VOID       *Context
  )
{
  EFI_STATUS             Status;
  STATIC UINT8           CallCount      = 0;
  CONFIG_VAR_LIST_ENTRY  *VarList       = NULL;
  UINTN                  VarListCount   = 0;
  UINTN                  Index;

  // locate protocol
  Status = gBS->LocateProtocol (&gDfciSettingsProviderSupportProtocolGuid, NULL, (VOID **)&mSettingProviderProtocol);
  if (EFI_ERROR (Status)) {
    if ((CallCount++ != 0) || (Status != EFI_NOT_FOUND)) {
      DEBUG ((DEBUG_ERROR, "%a() - Failed to locate gDfciSettingsProviderSupportProtocolGuid in notify.  Status = %r\n", __FUNCTION__, Status));
    }

    return;
  }

  Status = gBS->LocateProtocol (&gEdkiiVariablePolicyProtocolGuid, NULL, (VOID **)&mVariablePolicy);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a - Locating Variable Policy failed - %r\n", __FUNCTION__, Status));
    goto Done;
  }

  Status = RetrieveActiveConfigVarList (&VarList, &VarListCount);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a - Retrieving Variable List failed - %r\n", __FUNCTION__, Status));
    goto Done;
  }

  for (Index = 0; Index < VarListCount; Index++) {
    // Using default blob to initialize individual setting providers
    Status = RegisterSingleConfigVariable (&VarList[Index]);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a - Failed to register single config variable - %r\n", __FUNCTION__, Status));
      break;
    }
  }

Done:
  if (VarList != NULL) {
    for (Index = 0; Index < VarListCount; Index++) {
      // Also free the data and name
      FreePool (VarList[Index].Name);
      FreePool (VarList[Index].Data);
    }

    FreePool (VarList);
  }

  return;
}

/**
  This driver will attempt to register setting provider for setup variable
  (configuration data) based on platform supplied data iterator.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point always return success.
**/
EFI_STATUS
EFIAPI
ConfDataSettingProviderEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_EVENT  SettingsProviderSupportInstallEvent              = NULL;
  VOID       *SettingsProviderSupportInstallEventRegistration = NULL;

  // Install callback on the SettingsManager gDfciSettingsProviderSupportProtocolGuid protocol
  SettingsProviderSupportInstallEvent =
    EfiCreateProtocolNotifyEvent (
      &gDfciSettingsProviderSupportProtocolGuid,
      TPL_CALLBACK,
      SettingsProviderSupportProtocolNotify,
      NULL,
      &SettingsProviderSupportInstallEventRegistration
      );

  DEBUG ((DEBUG_INFO, "%a - Event Registered - %p.\n", __FUNCTION__, SettingsProviderSupportInstallEvent));

  return EFI_SUCCESS;
}
