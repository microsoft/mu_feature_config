/** @file
  Settings provider driver to register configuration data setter and getters.

  Copyright (c) 2017 - 2020, Intel Corporation. All rights reserved.<BR>
  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <DfciSystemSettingTypes.h>

#include <Protocol/DfciSettingsProvider.h>
#include <Protocol/Policy.h>

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

/**
  Get the default value of a single setting from UEFI FV.
  This getter will serialize default configuration setting
  to printable strings to be used in Config App.

  @param This           Seting Provider
  @param ValueSize      IN=Size of location to store value
                        OUT=Size of value stored
  @param DefaultValue   Output parameter for the settings default value.
                        The type and size is based on the provider type
                        and must be allocated by the caller.

  @retval EFI_SUCCESS           If the default could be returned.
  @retval EFI_BUFFER_TOO_SMALL  If the ValueSize on input is too small
  @retval ERROR                 Error
**/
EFI_STATUS
EFIAPI
ConfDataGetDefault (
  IN  CONST DFCI_SETTING_PROVIDER     *This,
  IN  OUT   UINTN                     *ValueSize,
  OUT       UINT8                     *Value
)
{
  UINTN       DataSize = 0;
  VOID        *Data = NULL;
  EFI_STATUS  Status;

  if ((This == NULL) || (This->Id == NULL) || (Value == NULL) || (ValueSize == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (0 != AsciiStrnCmp (This->Id, DFCI_OEM_SETTING_ID__CONF, DFCI_MAX_ID_LEN)) {
    DEBUG ((DEBUG_ERROR, "%a was called with incorrect Provider Id (%a)\n", __FUNCTION__, This->Id));
    return EFI_UNSUPPORTED;
  }

  // Then populate the slot with default one from FV.
  Status = GetSectionFromAnyFv (
             PcdGetPtr(PcdConfigPolicyVariableGuid),
             EFI_SECTION_RAW,
             0,
             (VOID **)&Data,
             &DataSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a Failed to get default settings (%r)\n", __FUNCTION__, Status));
    goto Exit;
  }

  Status = DumpConfigData (Data, DataSize, (CHAR8*)Value, ValueSize);
  if (Status == EFI_BUFFER_TOO_SMALL) {
    DEBUG ((DEBUG_WARN, "%a Prepared buffer too small, expecting 0x%x.\n", __FUNCTION__, *ValueSize));
  } else if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a Failed to print settings into buffer (%r)\n", __FUNCTION__, Status));
    goto Exit;
  }

Exit:
  if (Data != NULL) {
    FreePool (Data);
  }
  return Status;
}

/**
  Get a single setting from variable storage and serial content
  to printable string.

  @param This       Seting Provider
  @param ValueSize  IN=Size of location to store value
                    OUT=Size of value stored
  @param Value      Output parameter for the setting value.
                    The type and size is based on the provider type
                    and must be allocated by the caller.
  @retval EFI_SUCCESS           If setting could be retrieved.
  @retval EFI_BUFFER_TOO_SMALL  If the ValueSize on input is too small
  @retval ERROR                 Error
**/
EFI_STATUS
EFIAPI
ConfDataGet (
  IN CONST DFCI_SETTING_PROVIDER     *This,
  IN OUT   UINTN                     *ValueSize,
  OUT      UINT8                     *Value
)
{
  UINT32      Attr = 0;
  UINTN       DataSize = 0;
  VOID        *Data = NULL;
  EFI_STATUS  Status;

  if ((This == NULL) || (Value == NULL) || (ValueSize == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (0 != AsciiStrnCmp (This->Id, DFCI_OEM_SETTING_ID__CONF, DFCI_MAX_ID_LEN)) {
    DEBUG((DEBUG_ERROR, "%a was called with incorrect Provider Id (0x%X)\n", __FUNCTION__, This->Id));
    return EFI_UNSUPPORTED;
  }

  Status = gRT->GetVariable (
                  CDATA_NV_VAR_NAME,
                  PcdGetPtr(PcdConfigPolicyVariableGuid),
                  &Attr,
                  &DataSize,
                  NULL
                  );

  if (Status == EFI_NOT_FOUND) {
    // Simply grab the default one if no luck from var storage
    Status = ConfDataGetDefault (This, ValueSize, Value);
    if (Status == EFI_BUFFER_TOO_SMALL) {
      DEBUG((DEBUG_WARN, "%a Prepared buffer too small, expecting 0x%x.\n", __FUNCTION__, *ValueSize));
    } else if (EFI_ERROR (Status)){
      DEBUG((DEBUG_ERROR, "%a failed to fetch default settings - %r\n", __FUNCTION__, Status));
    }
    goto Exit;
  } else if (Status != EFI_BUFFER_TOO_SMALL) {
    DEBUG((DEBUG_ERROR, "%a unexpected result when fetching settings from var storage - %r\n", __FUNCTION__, Status));
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

  Data = AllocatePool (DataSize);
  Status = gRT->GetVariable (
                  CDATA_NV_VAR_NAME,
                  PcdGetPtr(PcdConfigPolicyVariableGuid),
                  &Attr,
                  &DataSize,
                  Data
                  );
  if (EFI_ERROR (Status)){
    DEBUG((DEBUG_ERROR, "%a failed to fetch settings from storage - %r\n", __FUNCTION__, Status));
    goto Exit;
  }

  Status = DumpConfigData (Data, DataSize, (CHAR8*)Value, ValueSize);
  if (Status == EFI_BUFFER_TOO_SMALL) {
    DEBUG((DEBUG_WARN, "%a Prepared buffer too small for current setting, expecting 0x%x.\n", __FUNCTION__, *ValueSize));
  } else if (EFI_ERROR (Status)){
    DEBUG ((DEBUG_ERROR, "%a failed to print settings from current settings - %r\n", __FUNCTION__, Status));
    goto Exit;
  }

Exit:
  if (Data != NULL) {
    FreePool (Data);
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
ConfDataSet (
  IN  CONST DFCI_SETTING_PROVIDER     *This,
  IN        UINTN                      ValueSize,
  IN  CONST UINT8                     *Value,
  OUT DFCI_SETTING_FLAGS              *Flags
)
{
  EFI_STATUS  Status = EFI_SUCCESS;

  if ((This == NULL) || (Flags == NULL) || (Value == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  *Flags = 0;

  if (0 != AsciiStrnCmp (This->Id, DFCI_OEM_SETTING_ID__CONF, DFCI_MAX_ID_LEN))
  {
    DEBUG ((DEBUG_ERROR, "ConfDataSet was called with incorrect Provider Id (%a)\n", This->Id));
    return EFI_UNSUPPORTED;
  }

  // Just add the new settings to the variable store.
  Status = gRT->SetVariable (CDATA_NV_VAR_NAME,
                             PcdGetPtr(PcdConfigPolicyVariableGuid),
                             CDATA_NV_VAR_ATTR,
                             ValueSize,
                             (VOID *) Value);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to set variable size %x. Code = %r\n", ValueSize, Status));
    return Status;
  }

  if (!EFI_ERROR (Status)) {
    *Flags |= DFCI_SETTING_FLAGS_OUT_REBOOT_REQUIRED;
  }

  return Status;
}

/**
  Set configuration to default value from UEFI FV.

  @param This          Seting Provider protocol

  @retval EFI_SUCCESS  default set
  @retval ERROR        Error
**/
EFI_STATUS
EFIAPI
ConfDataSetDefault (
  IN  CONST DFCI_SETTING_PROVIDER     *This
  )
{
  EFI_STATUS    Status;
  UINT32        Attr = 0;
  UINTN         DataSize = 0;
  VOID          *Data = NULL;

  if (This == NULL)
  {
    return EFI_INVALID_PARAMETER;
  }

  if (0 != AsciiStrnCmp (This->Id, DFCI_OEM_SETTING_ID__CONF, DFCI_MAX_ID_LEN)) {
    DEBUG ((DEBUG_ERROR, "%a was called with incorrect Provider Id (%a)\n", __FUNCTION__, This->Id));
    return EFI_UNSUPPORTED;
  }

  Status = gRT->GetVariable (
                  CDATA_NV_VAR_NAME,
                  PcdGetPtr(PcdConfigPolicyVariableGuid),
                  &Attr,
                  &DataSize,
                  NULL
                  );
  if (Status == EFI_BUFFER_TOO_SMALL) {
    // If there is already something on the flash, delete it.
    DEBUG ((DEBUG_WARN, "%a found existing settings from variable storage of size 0x%x, deleting it...\n", __FUNCTION__, DataSize));
    gRT->SetVariable (
           CDATA_NV_VAR_NAME,
           PcdGetPtr (PcdConfigPolicyVariableGuid),
           Attr,
           0,
           NULL
           );
  } else if (Status != EFI_NOT_FOUND) {
    DEBUG ((DEBUG_ERROR, "%a Unexpected result returned from variable settings - %r...\n", __FUNCTION__, Status));
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

  // Then populate the slot with default one from FV.
  Status = GetSectionFromAnyFv (
              PcdGetPtr(PcdConfigPolicyVariableGuid),
              EFI_SECTION_RAW,
              0,
              (VOID **)&Data,
              &DataSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a Failed to locate variable blob from FV, either %r\n", __FUNCTION__, Status));
    ASSERT (FALSE);
    goto Exit;
  }

  Status = gRT->SetVariable (CDATA_NV_VAR_NAME,
                             PcdGetPtr(PcdConfigPolicyVariableGuid),
                             CDATA_NV_VAR_ATTR,
                             DataSize,
                             (VOID *) Data);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a Failed to store variable blob to flash %r\n", __FUNCTION__, Status));
    ASSERT (FALSE);
    goto Exit;
  }

Exit:
  if (Data != NULL) {
    FreePool (Data);
  }
  return Status;
}

DFCI_SETTING_PROVIDER mSettingsProvider = {
  DFCI_OEM_SETTING_ID__CONF,
  DFCI_SETTING_TYPE_BINARY,
  DFCI_SETTING_FLAGS_OUT_REBOOT_REQUIRED,
  (DFCI_SETTING_PROVIDER_SET) ConfDataSet,
  (DFCI_SETTING_PROVIDER_GET) ConfDataGet,
  (DFCI_SETTING_PROVIDER_GET_DEFAULT) ConfDataGetDefault,
  (DFCI_SETTING_PROVIDER_SET_DEFAULT) ConfDataSetDefault
};

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
  IN  EFI_EVENT       Event,
  IN  VOID            *Context
)
{

  EFI_STATUS                              Status;
  DFCI_SETTING_PROVIDER_SUPPORT_PROTOCOL  *sp;
  STATIC UINT8                            CallCount = 0;

  //locate protocol
  Status = gBS->LocateProtocol (&gDfciSettingsProviderSupportProtocolGuid, NULL, (VOID**)&sp);
  if (EFI_ERROR (Status))
  {
    if ((CallCount++ != 0) || (Status != EFI_NOT_FOUND))
    {
      DEBUG ((DEBUG_ERROR, "%a() - Failed to locate gDfciSettingsProviderSupportProtocolGuid in notify.  Status = %r\n", __FUNCTION__, Status));
    }
    return ;
  }

  //call function
  DEBUG ((DEBUG_INFO, "Registering configuration Setting Provider\n"));
  Status = sp->RegisterProvider (sp, &mSettingsProvider);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to Register.  Status = %r\n", Status));
  }
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
  EFI_EVENT SettingsProviderSupportInstallEvent = NULL;
  VOID      *SettingsProviderSupportInstallEventRegistration = NULL;

  //Install callback on the SettingsManager gDfciSettingsProviderSupportProtocolGuid protocol
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
