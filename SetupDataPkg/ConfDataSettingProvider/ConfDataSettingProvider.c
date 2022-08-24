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
#include <Library/ConfigVariableListLib.h>

DFCI_SETTING_PROVIDER_SUPPORT_PROTOCOL  *mSettingProviderProtocol = NULL;
EDKII_VARIABLE_POLICY_PROTOCOL          *mVariablePolicy          = NULL;

/**
  Find and return the default value of a setting from UEFI FV.

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
GetDefaultConfigDataBin (
  IN OUT  UINTN  *ValueSize,
  OUT     UINT8  *Value
  )
{
  UINTN       DataSize = 0;
  VOID        *Data    = NULL;
  EFI_STATUS  Status;

  // Then populate the slot with default one from FV.
  Status = GetSectionFromAnyFv (
             &gSetupConfigPolicyVariableGuid,
             EFI_SECTION_RAW,
             0,
             (VOID **)&Data,
             &DataSize
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a Failed to get default settings (%r)\n", __FUNCTION__, Status));
    goto Exit;
  }

  if (DataSize > *ValueSize) {
    Status = EFI_BUFFER_TOO_SMALL;
    goto Exit;
  } else {
    CopyMem (Value, Data, DataSize);
  }

Exit:
  if (Data != NULL) {
    FreePool (Data);
  }

  *ValueSize = DataSize;
  return Status;
}

/**
  Get the default value of configuration settings from UEFI FV.
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
ConfDataGetDefault (
  IN  CONST DFCI_SETTING_PROVIDER  *This,
  IN  OUT   UINTN                  *ValueSize,
  OUT       UINT8                  *Value
  )
{
  EFI_STATUS  Status;

  if ((This == NULL) || (This->Id == NULL) || (ValueSize == NULL) || ((*ValueSize != 0) && (Value == NULL))) {
    return EFI_INVALID_PARAMETER;
  }

  if (0 != AsciiStrnCmp (This->Id, DFCI_OEM_SETTING_ID__CONF, DFCI_MAX_ID_LEN)) {
    DEBUG ((DEBUG_ERROR, "%a was called with incorrect Provider Id (%a)\n", __FUNCTION__, This->Id));
    return EFI_UNSUPPORTED;
  }

  Status = GetDefaultConfigDataBin (ValueSize, Value);
  return Status;
}

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
  )
{
  if ((This == NULL) || (This->Id == NULL) || (ValueSize == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (0 != AsciiStrnCmp (This->Id, DFCI_OEM_SETTING_ID__CONF, DFCI_MAX_ID_LEN)) {
    DEBUG ((DEBUG_ERROR, "%a was called with incorrect Provider Id (0x%X)\n", __FUNCTION__, This->Id));
    return EFI_UNSUPPORTED;
  }

  // This operation is not supported
  return ConfDataGetDefault (This, ValueSize, Value);
}

// Helper functions to set a per-tag based data
STATIC
EFI_STATUS
EFIAPI
SetSingleConfigData (
  UINT32  Tag,
  VOID    *Buffer,
  UINTN   BufferSize
  )
{
  CHAR16      *TempUnicodeId;
  UINTN       Size;
  EFI_STATUS  Status;

  Size          = sizeof (SINGLE_SETTING_PROVIDER_START) + 2 * sizeof (Tag);
  TempUnicodeId = AllocatePool (Size * 2);
  if (TempUnicodeId == NULL) {
    DEBUG ((DEBUG_ERROR, "Failed to allocate buffer for ID 0x%x.\n", Tag));
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  UnicodeSPrintAsciiFormat (TempUnicodeId, Size * 2, SINGLE_SETTING_PROVIDER_TEMPLATE, Tag);

  Status = gRT->SetVariable (TempUnicodeId, &gSetupConfigPolicyVariableGuid, CDATA_NV_VAR_ATTR, BufferSize, Buffer);

Exit:
  if (TempUnicodeId != NULL) {
    FreePool (TempUnicodeId);
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
  IN  CONST DFCI_SETTING_PROVIDER  *This,
  IN        UINTN                  ValueSize,
  IN  CONST UINT8                  *Value,
  OUT DFCI_SETTING_FLAGS           *Flags
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  if ((This == NULL) || (This->Id == NULL) || (Flags == NULL) || (Value == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *Flags = 0;

  if (0 != AsciiStrnCmp (This->Id, DFCI_OEM_SETTING_ID__CONF, DFCI_MAX_ID_LEN)) {
    DEBUG ((DEBUG_ERROR, "ConfDataSet was called with incorrect Provider Id (%a)\n", This->Id));
    return EFI_UNSUPPORTED;
  }

  Status = IterateConfData (Value, SetSingleConfigData);
  if (!EFI_ERROR (Status)) {
    *Flags |= DFCI_SETTING_FLAGS_OUT_REBOOT_REQUIRED;
  }

  return Status;
}

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
  )
{
  EFI_STATUS  Status;
  UINTN       Size  = 0;
  VOID        *Data = NULL;

  if ((This == NULL) || (This->Id == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (0 != AsciiStrnCmp (This->Id, DFCI_OEM_SETTING_ID__CONF, DFCI_MAX_ID_LEN)) {
    DEBUG ((DEBUG_ERROR, "%a was called with incorrect Provider Id (%a)\n", __FUNCTION__, This->Id));
    return EFI_UNSUPPORTED;
  }

  Size   = 0;
  Status = GetDefaultConfigDataBin (&Size, Data);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    goto Done;
  }

  Data = AllocatePool (Size);
  if (Data == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  Status = GetDefaultConfigDataBin (&Size, Data);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  Status = IterateConfData (Data, SetSingleConfigData);

Done:
  if (Data != NULL) {
    FreePool (Data);
  }

  return Status;
}

DFCI_SETTING_PROVIDER  mSettingsProvider = {
  DFCI_OEM_SETTING_ID__CONF,
  DFCI_SETTING_TYPE_BINARY,
  DFCI_SETTING_FLAGS_OUT_REBOOT_REQUIRED,
  (DFCI_SETTING_PROVIDER_SET)ConfDataSet,
  (DFCI_SETTING_PROVIDER_GET)ConfDataGet,
  (DFCI_SETTING_PROVIDER_GET_DEFAULT)ConfDataGetDefault,
  (DFCI_SETTING_PROVIDER_SET_DEFAULT)ConfDataSetDefault
};

/**
  Set new runtime value to variable storage.

  @param This      Provider Setting
  @param Value     a pointer to the variable list
  @param ValueSize Size of the data for this setting.
  @param Flags     Informational Flags passed to the SET and/or Returned as a result of the set

  @retval EFI_SUCCESS If setting could be set.  Check flags for other info (reset required, etc)
  @retval Error       Setting not set.
**/
EFI_STATUS
EFIAPI
RuntimeDataSet (
  IN  CONST DFCI_SETTING_PROVIDER  *This,
  IN        UINTN                  ValueSize,
  IN  CONST UINT8                  *Value,
  OUT DFCI_SETTING_FLAGS           *Flags
  )
{
  EFI_STATUS            Status   = EFI_SUCCESS;
  CHAR16                *VarName = NULL;
  CHAR16                *name;
  EFI_GUID              *Guid;
  UINT32                Attributes;
  CHAR8                 *Data;
  UINT32                CRC32;
  UINT32                LenToCRC32;
  UINT32                CalcCRC32 = 0;
  CONFIG_VAR_LIST_HDR   *VarList;
  UINT32                ListIndex = 0;

  if ((This == NULL) || (This->Id == NULL) || (Flags == NULL) || (Value == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *Flags = 0;

  if (0 != AsciiStrnCmp (This->Id, DFCI_OEM_SETTING_ID__RUNTIME, DFCI_MAX_ID_LEN)) {
    DEBUG ((DEBUG_ERROR, "RuntimeDataSet was called with incorrect Provider Id (%a)\n", This->Id));
    return EFI_UNSUPPORTED;
  }

  while (ListIndex < ValueSize) {
    // index into variable list
    VarList = (CONFIG_VAR_LIST_HDR *)(Value + ListIndex);

    if (ListIndex + sizeof (*VarList) + VarList->NameSize + VarList->DataSize + sizeof (*Guid) +
        sizeof (Attributes) + sizeof (CRC32) > ValueSize)
    {
      // the NameSize and DataSize have bad values and are pushing us past the end of the binary
      DEBUG ((DEBUG_ERROR, "Runtime Settings had bad NameSize or DataSize, unable to process all settings\n"));
      break;
    }

    /*
     * Var List is in DmpStore format:
     *
     *  struct {
     *    CONFIG_VAR_LIST_HDR VarList;
     *    CHAR16 Name[VarList->NameSize/2];
     *    EFI_GUID Guid;
     *    UINT32 Attributes;
     *    CHAR8 Data[VarList->DataSize];
     *    UINT32 CRC32; // CRC32 of all bytes from VarList to end of Data
     *  }
     */
    name       = (CHAR16 *)(VarList + 1);
    Guid       = (EFI_GUID *)((CHAR8 *)name + VarList->NameSize);
    Attributes = *(UINT32 *)(Guid + 1);
    Data       = (CHAR8 *)Guid + sizeof (*Guid) + sizeof (Attributes);
    CRC32      = *(UINT32 *)(Data + VarList->DataSize);
    LenToCRC32 = sizeof (*VarList) + VarList->NameSize + VarList->DataSize + sizeof (*Guid) + sizeof (Attributes);

    // on next iteration, skip past this variable
    ListIndex += LenToCRC32 + sizeof (CRC32);

    // validate CRC32
    CalcCRC32 = CalculateCrc32 (VarList, LenToCRC32);

    if (CalcCRC32 != CRC32) {
      // CRC didn't match, drop this variable, continue to the next
      DEBUG ((DEBUG_ERROR, "DFCI Runtime Settings Provider failed CRC check, skipping applying setting: %s Status: %r \
      Received CRC32: %u Calculated CRC32: %u\n", name, Status, CRC32, CalcCRC32));
      continue;
    }

    VarName = AllocatePool (VarList->NameSize);
    if (VarName == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      return Status;
    }

    // Copy non-16 bit aligned name to VarName, not using StrCpyS functions as they assert on non-16 bit alignment
    CopyMem (VarName, name, VarList->NameSize);

    // write variable directly to var storage
    Status = gRT->SetVariable (
                    VarName,
                    Guid,
                    Attributes,
                    VarList->DataSize,
                    Data
                    );

    if (EFI_ERROR (Status)) {
      // failed to set variable, continue to try with other variables
      DEBUG ((DEBUG_ERROR, "Failed to set Runtime Setting %s, continuing to try next variables\n", VarName));
    }

    FreePool (VarName);
  }

  return Status;
}

/**
  It is not supported to get the full Runtime Setting list. Instead,
  each individual Runtime Settings Provider can fetch the setting.

  @param This       Setting Provider
  @param ValueSize  IN=Size of location to store value
                    OUT=Size of value stored
  @param Value      Output parameter for the setting value.
                    The type and size is based on the provider type
                    and must be allocated by the caller.
  @retval EFI_UNSUPPORTED   This is not supported for Runtime Settings
**/
EFI_STATUS
EFIAPI
RuntimeDataGet (
  IN CONST DFCI_SETTING_PROVIDER  *This,
  IN OUT   UINTN                  *ValueSize,
  OUT      UINT8                  *Value
  )
{
  return EFI_UNSUPPORTED;
}

/**
  Default values of Runtime Settings are not supported.

  @param This           Setting Provider
  @param ValueSize      IN=Size of location to store value
                        OUT=Size of value stored
  @param Value          Output parameter for the settings default value.
                        The type and size is based on the provider type
                        and must be allocated by the caller.

  @retval EFI_UNSUPPORTED           This is not supported for Runtime Settings
**/
EFI_STATUS
EFIAPI
RuntimeDataGetDefault (
  IN  CONST DFCI_SETTING_PROVIDER  *This,
  IN  OUT   UINTN                  *ValueSize,
  OUT       UINT8                  *Value
  )
{
  return EFI_UNSUPPORTED;
}

/**
  It is not supported to get the full Runtime Setting list. Instead,
  each individual Runtime Settings Provider can fetch the setting.

  @param This          Setting Provider protocol

  @retval EFI_UNSUPPORTED         This is not supported for Runtime Settings
**/
EFI_STATUS
EFIAPI
RuntimeDataSetDefault (
  IN  CONST DFCI_SETTING_PROVIDER  *This
  )
{
  return EFI_UNSUPPORTED;
}

DFCI_SETTING_PROVIDER  mRuntimeSettingsProvider = {
  DFCI_OEM_SETTING_ID__RUNTIME,
  DFCI_SETTING_TYPE_BINARY,
  DFCI_SETTING_FLAGS_OUT_REBOOT_REQUIRED,
  (DFCI_SETTING_PROVIDER_SET)RuntimeDataSet,
  (DFCI_SETTING_PROVIDER_GET)RuntimeDataGet,
  (DFCI_SETTING_PROVIDER_GET_DEFAULT)RuntimeDataGetDefault,
  (DFCI_SETTING_PROVIDER_SET_DEFAULT)RuntimeDataSetDefault
};

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
  )
{
  UINT32  Temp;
  UINTN   Offset;
  CHAR8   Char;
  UINTN   TotalSize = sizeof (SINGLE_SETTING_PROVIDER_START) + sizeof (UINT32) * 2;

  if ((TagId == NULL) ||
      (IdString == NULL) ||
      (AsciiStrSize (IdString) != TotalSize) ||
      !CompareMem (IdString, SINGLE_SETTING_PROVIDER_START, sizeof (SINGLE_SETTING_PROVIDER_START)))
  {
    return EFI_INVALID_PARAMETER;
  }

  // Till here, we know that we have 8 bytes left to process
  Offset = sizeof (SINGLE_SETTING_PROVIDER_START) - 1;

  Temp = 0;
  while (Offset < (TotalSize - 1)) {
    Char = IdString[Offset];
    if ((Char >= 'A') && (Char <= 'F')) {
      Temp = (Temp << 4) + (Char - 'A' + 0x0A);
    } else if ((Char >= '0') && (Char <= '9')) {
      Temp = (Temp << 4) + (Char - '0');
    } else {
      // Do not support other characters from here...
      return EFI_INVALID_PARAMETER;
    }

    Offset++;
  }

  *TagId = (UINT32)Temp;

  return EFI_SUCCESS;
}

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
  UINT32        TagId;
  VOID          *DefaultBuffer = NULL;
  CDATA_HEADER  *TagHdrBuffer;
  VOID          *TagBuffer;
  EFI_STATUS    Status;
  UINTN         Size;
  CHAR16        *VarName = NULL;

  if ((This == NULL) || (This->Id == NULL)) {
    Status = EFI_INVALID_PARAMETER;
    goto Done;
  }

  Status = GetTagIdFromDfciId (This->Id, &TagId);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  Size   = 0;
  Status = GetDefaultConfigDataBin (&Size, DefaultBuffer);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    goto Done;
  }

  DefaultBuffer = AllocatePool (Size);
  if (DefaultBuffer == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  Status = GetDefaultConfigDataBin (&Size, DefaultBuffer);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  TagHdrBuffer = FindConfigHdrByTag (DefaultBuffer, TagId);
  if (TagHdrBuffer == NULL) {
    Status = EFI_NOT_FOUND;
    goto Done;
  }

  TagBuffer = FindConfigDataByTag (DefaultBuffer, TagId);
  if (TagBuffer == NULL) {
    Status = EFI_NOT_FOUND;
    goto Done;
  }

  Size    = AsciiStrLen (This->Id);
  VarName = AllocatePool ((Size + 1) * 2);
  if (VarName == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  AsciiStrToUnicodeStrS (This->Id, VarName, Size + 1);

  Status = gRT->SetVariable (
                  VarName,
                  &gSetupConfigPolicyVariableGuid,
                  CDATA_NV_VAR_ATTR,
                  (TagHdrBuffer->Length) - sizeof (*TagHdrBuffer) - sizeof (CDATA_COND) * TagHdrBuffer->ConditionNum,
                  TagBuffer
                  );

Done:
  if (DefaultBuffer != NULL) {
    FreePool (DefaultBuffer);
  }

  if (VarName != NULL) {
    FreePool (VarName);
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
  EFI_STATUS    Status;
  UINT32        TagId;
  CDATA_HEADER  *TagHdrBuffer;
  VOID          *TagBuffer;
  VOID          *DefaultBuffer = NULL;
  UINTN         Size;

  if ((This == NULL) || (This->Id == NULL) || (ValueSize == NULL) || ((*ValueSize != 0) && (Value == NULL))) {
    Status = EFI_INVALID_PARAMETER;
    goto Done;
  }

  Status = GetTagIdFromDfciId (This->Id, &TagId);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  Size   = 0;
  Status = GetDefaultConfigDataBin (&Size, DefaultBuffer);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    goto Done;
  }

  DefaultBuffer = AllocatePool (Size);
  if (DefaultBuffer == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  Status = GetDefaultConfigDataBin (&Size, DefaultBuffer);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  TagHdrBuffer = FindConfigHdrByTag (DefaultBuffer, TagId);
  if (TagHdrBuffer == NULL) {
    Status = EFI_NOT_FOUND;
    goto Done;
  }

  TagBuffer = FindConfigDataByTag (DefaultBuffer, TagId);
  if (TagBuffer == NULL) {
    Status = EFI_NOT_FOUND;
    goto Done;
  }

  Size = (TagHdrBuffer->Length) - sizeof (*TagHdrBuffer) - sizeof (CDATA_COND) * TagHdrBuffer->ConditionNum;
  if (*ValueSize < Size) {
    Status = EFI_BUFFER_TOO_SMALL;
  } else {
    CopyMem (Value, TagBuffer, Size);
    Status = EFI_SUCCESS;
  }

  *ValueSize = Size;

Done:
  if (DefaultBuffer != NULL) {
    FreePool (DefaultBuffer);
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
  UINT32      TagId;
  EFI_STATUS  Status;
  CHAR16      *VarName = NULL;
  UINTN       Size;

  if ((This == NULL) || (This->Id == NULL) || (Value == NULL) || (ValueSize == 0) || (Flags == NULL)) {
    Status = EFI_INVALID_PARAMETER;
    goto Done;
  }

  Status = GetTagIdFromDfciId (This->Id, &TagId);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  Size    = AsciiStrLen (This->Id);
  VarName = AllocatePool ((Size + 1) * 2);
  if (VarName == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  AsciiStrToUnicodeStrS (This->Id, VarName, Size + 1);

  // Now set it to non-volatile variable
  Status = gRT->SetVariable (VarName, &gSetupConfigPolicyVariableGuid, CDATA_NV_VAR_ATTR, ValueSize, (VOID *)Value);

  if (!EFI_ERROR (Status)) {
    *Flags |= DFCI_SETTING_FLAGS_OUT_REBOOT_REQUIRED;
  }

Done:
  if (VarName != NULL) {
    FreePool (VarName);
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
  UINT32      TagId;
  EFI_STATUS  Status;
  CHAR16      *VarName = NULL;
  UINTN       Size;

  if ((This == NULL) || (This->Id == NULL) || (ValueSize == NULL) || ((*ValueSize != 0) && (Value == NULL))) {
    Status = EFI_INVALID_PARAMETER;
    goto Done;
  }

  Status = GetTagIdFromDfciId (This->Id, &TagId);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  Size    = AsciiStrLen (This->Id);
  VarName = AllocatePool ((Size + 1) * 2);
  if (VarName == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  AsciiStrToUnicodeStrS (This->Id, VarName, Size + 1);

  Status = gRT->GetVariable (VarName, &gSetupConfigPolicyVariableGuid, NULL, ValueSize, (VOID *)Value);

Done:
  if (VarName != NULL) {
    FreePool (VarName);
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
RegisterSingleTag (
  UINT32  Tag,
  VOID    *Buffer,
  UINTN   BufferSize
  )
{
  EFI_STATUS             Status;
  UINTN                  Size;
  DFCI_SETTING_PROVIDER  *Setting;
  CHAR8                  *TempAsciiId;
  CHAR16                 *TempUnicodeId;

  if ((mVariablePolicy == NULL) || (mSettingProviderProtocol == NULL)) {
    DEBUG ((DEBUG_ERROR, "Either setting access (%p) or variable policy policy (%p) is not ready!!!\n", mSettingProviderProtocol, mVariablePolicy));
    Status = EFI_NOT_READY;
    goto Exit;
  }

  Setting = AllocateCopyPool (sizeof (DFCI_SETTING_PROVIDER), &SingleSettingProviderTemplate);
  if (Setting == NULL) {
    DEBUG ((DEBUG_ERROR, "Failed to allocate buffer for setting 0x%x.\n", Tag));
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  Size        = sizeof (SINGLE_SETTING_PROVIDER_START) + 2 * sizeof (Tag);
  TempAsciiId = AllocatePool (Size);
  if (TempAsciiId == NULL) {
    DEBUG ((DEBUG_ERROR, "Failed to allocate buffer for ID 0x%x.\n", Tag));
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  AsciiSPrint (TempAsciiId, Size, "%a%08X", SINGLE_SETTING_PROVIDER_START, Tag);
  Setting->Id = TempAsciiId;
  Status      = mSettingProviderProtocol->RegisterProvider (mSettingProviderProtocol, Setting);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to Register for ID 0x%x.  Status = %r\n", Tag, Status));
  }

  TempUnicodeId = AllocatePool (Size * 2);
  if (TempUnicodeId == NULL) {
    DEBUG ((DEBUG_ERROR, "Failed to allocate unicode buffer for ID 0x%x.\n", Tag));
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  AsciiStrToUnicodeStrS (TempAsciiId, TempUnicodeId, Size);

  Size   = 0;
  Status = gRT->GetVariable (TempUnicodeId, &gSetupConfigPolicyVariableGuid, NULL, &Size, NULL);
  if (Status == EFI_NOT_FOUND) {
    Status = gRT->SetVariable (TempUnicodeId, &gSetupConfigPolicyVariableGuid, CDATA_NV_VAR_ATTR, BufferSize, Buffer);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Initializing variable %s failed - %r.\n", TempUnicodeId, Status));
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

  // The new configuration, if any, should only grow in size
  if (BufferSize > MAX_UINT32) {
    DEBUG ((DEBUG_ERROR, "Configuration too large %llx.\n", BufferSize));
    Status = EFI_BUFFER_TOO_SMALL;
    goto Exit;
  }

  Status = RegisterVarStateVariablePolicy (
             mVariablePolicy,
             &gSetupConfigPolicyVariableGuid,
             TempUnicodeId,
             (UINT32)BufferSize,
             VARIABLE_POLICY_NO_MAX_SIZE,
             CDATA_NV_VAR_ATTR,
             (UINT32) ~CDATA_NV_VAR_ATTR,
             &gMuVarPolicyDxePhaseGuid,
             READY_TO_BOOT_INDICATOR_VAR_NAME,
             PHASE_INDICATOR_SET
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a - Registering Variable Policy for Target Variable %s failed - %r\n", __FUNCTION__, TempUnicodeId, Status));
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

  if (TempUnicodeId != NULL) {
    FreePool (TempUnicodeId);
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
  EFI_STATUS    Status;
  STATIC UINT8  CallCount = 0;
  UINTN         Size;
  VOID          *DefaultBuffer = NULL;

  // locate protocol
  Status = gBS->LocateProtocol (&gDfciSettingsProviderSupportProtocolGuid, NULL, (VOID **)&mSettingProviderProtocol);
  if (EFI_ERROR (Status)) {
    if ((CallCount++ != 0) || (Status != EFI_NOT_FOUND)) {
      DEBUG ((DEBUG_ERROR, "%a() - Failed to locate gDfciSettingsProviderSupportProtocolGuid in notify.  Status = %r\n", __FUNCTION__, Status));
    }

    return;
  }

  // call function
  DEBUG ((DEBUG_INFO, "Registering configuration Setting Provider\n"));
  Status = mSettingProviderProtocol->RegisterProvider (mSettingProviderProtocol, &mSettingsProvider);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to Register.  Status = %r\n", Status));
  }

  // register runtime settings provider
  DEBUG ((DEBUG_INFO, "Registering runtime Setting Provider\n"));
  Status = mSettingProviderProtocol->RegisterProvider (mSettingProviderProtocol, &mRuntimeSettingsProvider);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to Register Runtime Settings.  Status = %r\n", Status));
  }

  Status = gBS->LocateProtocol (&gEdkiiVariablePolicyProtocolGuid, NULL, (VOID **)&mVariablePolicy);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a - Locating Variable Policy failed - %r\n", __FUNCTION__, Status));
    goto Done;
  }

  Size   = 0;
  Status = GetDefaultConfigDataBin (&Size, DefaultBuffer);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    goto Done;
  }

  DefaultBuffer = AllocatePool (Size);
  if (DefaultBuffer == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  Status = GetDefaultConfigDataBin (&Size, DefaultBuffer);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  // Using default blob to initialize individual setting providers
  Status = IterateConfData (DefaultBuffer, RegisterSingleTag);
  if (EFI_ERROR (Status)) {
    goto Done;
  }

Done:
  if (DefaultBuffer != NULL) {
    FreePool (DefaultBuffer);
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
