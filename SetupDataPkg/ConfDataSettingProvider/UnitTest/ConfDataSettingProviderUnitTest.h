/** @file
  Header file for unit test to include functions to be tested from
  Configuration Data Setting Provider module.

  Copyright (C) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef CONF_DATA_SETTING_PROVIDER_UNIT_TEST_H_
#define CONF_DATA_SETTING_PROVIDER_UNIT_TEST_H_

/**
  Get the default value of configuration settings from UEFI FV.
  This getter will serialize default configuration setting
  to printable strings to be used in Config App.

  @param This           Setting Provider
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
  IN  CONST DFCI_SETTING_PROVIDER  *This,
  IN  OUT   UINTN                  *ValueSize,
  OUT       UINT8                  *Value
  );

/**
  Get the full setting in binary from FV.

  @param This       Setting Provider
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
  IN CONST DFCI_SETTING_PROVIDER  *This,
  IN OUT   UINTN                  *ValueSize,
  OUT      UINT8                  *Value
  );

/**
  Set configuration to default value from UEFI FV.

  @param This          Setting Provider protocol

  @retval EFI_SUCCESS  default set
  @retval ERROR        Error
**/
EFI_STATUS
EFIAPI
ConfDataSetDefault (
  IN  CONST DFCI_SETTING_PROVIDER  *This
  );

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
  IN  CONST DFCI_SETTING_PROVIDER  *This,
  IN        UINTN                  ValueSize,
  IN  CONST UINT8                  *Value,
  OUT DFCI_SETTING_FLAGS           *Flags
  );

/**
  Get the default value of a single setting from UEFI FV.
  This getter will serialize default configuration setting
  to printable strings to be used in Config App.

  @param This           Setting Provider
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
SingleConfDataGetDefault (
  IN  CONST DFCI_SETTING_PROVIDER  *This,
  IN  OUT   UINTN                  *ValueSize,
  OUT       UINT8                  *Value
  );

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
  );

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
  );

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
  );

/*
  Helper function extract tag ID from single setting provider ID.

  @param IdString       Setting Provider ID, should be in the format
                        SINGLE_SETTING_PROVIDER_TEMPLATE
  @param TagId          Pointer to hold extracted TagId value in the
                        IdString.

  @retval EFI_SUCCESS             Tag ID extracted.
  @retval EFI_INVALID_PARAMETER   Input ID string does not match template,
                                  or the
*/
EFI_STATUS
GetTagIdFromDfciId (
  IN  DFCI_SETTING_ID_STRING  IdString,
  OUT UINT32                  *TagId
  );

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
  );

#endif // CONF_DATA_SETTING_PROVIDER_UNIT_TEST_H_
