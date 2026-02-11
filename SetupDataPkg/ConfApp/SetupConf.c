/** @file
  Test driver to locate conf data from variable storage and print all contained entries.

  Copyright (c) 2017 - 2020, Intel Corporation. All rights reserved.<BR>
  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <XmlTypes.h>

#include <Protocol/Policy.h>

#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/XmlTreeLib.h>
#include <Library/XmlTreeQueryLib.h>
#include <Library/SvdXmlSettingSchemaSupportLib.h>
#include <Library/PerformanceLib.h>
#include <Library/ConfigVariableListLib.h>
#include <Library/ConfigSystemModeLib.h>
#include <Library/ResetUtilityLib.h>

#include "ConfApp.h"
#include "SvdUsb/SvdUsb.h"

#define SETUP_CONF_STATE_OPTIONS  6

ConfAppKeyOptions  SetupConfStateOptions[SETUP_CONF_STATE_OPTIONS] = {
  {
    .KeyName             = L"1",
    .KeyNameTextAttr     = EFI_TEXT_ATTR (EFI_YELLOW, EFI_BLACK),
    .Description         = L"Update from USB Stick.",
    .DescriptionTextAttr = EFI_TEXT_ATTR (EFI_WHITE, EFI_BLACK),
    .UnicodeChar         = '1',
    .ScanCode            = SCAN_NULL,
    .EndState            = SetupConfUpdateUsb
  },
  {
    .KeyName             = L"2",
    .KeyNameTextAttr     = EFI_TEXT_ATTR (EFI_YELLOW, EFI_BLACK),
    .Description         = L"Update from Serial Port.",
    .DescriptionTextAttr = EFI_TEXT_ATTR (EFI_WHITE, EFI_BLACK),
    .UnicodeChar         = '2',
    .ScanCode            = SCAN_NULL,
    .EndState            = SetupConfUpdateSerialHint
  },
  {
    .KeyName             = L"3",
    .KeyNameTextAttr     = EFI_TEXT_ATTR (EFI_YELLOW, EFI_BLACK),
    .Description         = L"Dump Current Configuration.\n",
    .DescriptionTextAttr = EFI_TEXT_ATTR (EFI_WHITE, EFI_BLACK),
    .UnicodeChar         = '3',
    .ScanCode            = SCAN_NULL,
    .EndState            = SetupConfDumpSerial
  },
  {
    .KeyName             = L"h",
    .KeyNameTextAttr     = EFI_TEXT_ATTR (EFI_YELLOW, EFI_BLACK),
    .Description         = L"Reprint this menu.",
    .DescriptionTextAttr = EFI_TEXT_ATTR (EFI_WHITE, EFI_BLACK),
    .UnicodeChar         = 'h',
    .ScanCode            = SCAN_NULL,
    .EndState            = SetupConfInit
  },
  {
    .KeyName             = L"ESC",
    .KeyNameTextAttr     = EFI_TEXT_ATTR (EFI_YELLOW, EFI_BLACK),
    .Description         = L"Exit this menu and return to previous menu.",
    .DescriptionTextAttr = EFI_TEXT_ATTR (EFI_WHITE, EFI_BLACK),
    .UnicodeChar         = CHAR_NULL,
    .ScanCode            = SCAN_ESC,
    .EndState            = SetupConfExit
  }
};

typedef struct {
  UINT32        TagId;
  LIST_ENTRY    Link;
} CONFIG_TAG_LINK_HEADER;

SetupConfState_t  mSetupConfState  = SetupConfInit;
UINT16            *mConfDataBuffer = NULL;
UINTN             mConfDataOffset  = 0;
UINTN             mConfDataSize    = 0;
POLICY_PROTOCOL   *mPolicyProtocol = NULL;

EFI_STATUS
InspectDumpOutput (
  IN VOID   *Buffer,
  IN UINTN  BufferSize
  );

#ifndef UNIT_TEST_ENV
EFI_STATUS
InspectDumpOutput (
  IN VOID   *Buffer,
  IN UINTN  BufferSize
  )
{
  // Do very little check for real running environment
  if ((Buffer != NULL) && (BufferSize != 0)) {
    return EFI_SUCCESS;
  }

  return EFI_COMPROMISED_DATA;
}

#endif // UNIT_TEST_ENV

/**
  Helper internal function to reset all local variable in this file.
**/
STATIC
VOID
ResetGlobals (
  VOID
  )
{
  mSetupConfState = SetupConfInit;
  if (mConfDataBuffer) {
    FreePool (mConfDataBuffer);
    mConfDataBuffer = NULL;
  }

  SetupConfStateOptions[0].DescriptionTextAttr = EFI_TEXT_ATTR (EFI_WHITE, EFI_BLACK);
  SetupConfStateOptions[0].EndState            = SetupConfUpdateUsb;

  SetupConfStateOptions[1].DescriptionTextAttr = EFI_TEXT_ATTR (EFI_WHITE, EFI_BLACK);
  SetupConfStateOptions[1].EndState            = SetupConfUpdateSerialHint;

  mConfDataSize   = 0;
  mConfDataOffset = 0;
}

/**
  Helper function to print available selections to apply configuration data.

  @retval EFI_SUCCESS     This function executes successfully.
  @retval Others          The print functions returned errors.
**/
EFI_STATUS
PrintOptions (
  VOID
  )
{
  EFI_STATUS  Status;

  PrintScreenInit ();
  Print (L"Setup Configuration Options:\n");
  Print (L"\n");

  if (!IsSystemInManufacturingMode ()) {
    gST->ConOut->SetAttribute (gST->ConOut, EFI_TEXT_ATTR (EFI_YELLOW, EFI_BLACK));
    Print (L"Updating configuration will not take any effect per platform state:\n");
    SetupConfStateOptions[0].DescriptionTextAttr = EFI_TEXT_ATTR (EFI_DARKGRAY, EFI_BLACK);
    SetupConfStateOptions[0].EndState            = SetupConfError;

    SetupConfStateOptions[1].DescriptionTextAttr = EFI_TEXT_ATTR (EFI_DARKGRAY, EFI_BLACK);
    SetupConfStateOptions[1].EndState            = SetupConfError;
  }

  Status = PrintAvailableOptions (SetupConfStateOptions, SETUP_CONF_STATE_OPTIONS);
  if (EFI_ERROR (Status)) {
    ASSERT (FALSE);
  }

  return Status;
}

/**
  Set arbitrary SVD values to variable storage

  @param Value     a pointer to the variable list
  @param ValueSize Size of the data for this setting.

  @retval EFI_SUCCESS If setting could be set.
  @retval Error       Setting not set.
**/
STATIC
EFI_STATUS
WriteSVDSetting (
  IN  CONST UINT8  *Value,
  IN        UINTN  ValueSize
  )
{
  EFI_STATUS             Status             = EFI_SUCCESS;
  UINT32                 ListIndex          = 0;
  CONFIG_VAR_LIST_ENTRY  *ConfigVarListPtr  = NULL;
  UINTN                  ConfigVarListCount = 0;

  if ((Value == NULL) || (ValueSize == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = RetrieveActiveConfigVarList (Value, ValueSize, &ConfigVarListPtr, &ConfigVarListCount);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to extract all configuration elements - %r\n", Status));
    goto Done;
  }

  for (ListIndex = 0; ListIndex < ConfigVarListCount; ListIndex++) {
    // first delete the variable in case of size or attributes change, not validated here as this is only allowed
    // in manufacturing mode. Don't retrieve the status, if we fail to delete, try to write it anyway. If we fail
    // there, just log it and move on
    gRT->SetVariable (
           ConfigVarListPtr->Name,
           &ConfigVarListPtr->Guid,
           0,
           0,
           NULL
           );

    // write variable directly to var storage
    Status = gRT->SetVariable (
                    ConfigVarListPtr->Name,
                    &ConfigVarListPtr->Guid,
                    ConfigVarListPtr->Attributes,
                    ConfigVarListPtr->DataSize,
                    ConfigVarListPtr->Data
                    );

    if (EFI_ERROR (Status)) {
      // failed to set variable, continue to try with other variables
      DEBUG ((DEBUG_ERROR, "Failed to set SVD Setting %s, continuing to try next variables\n", ConfigVarListPtr->Name));
    }

    FreePool (ConfigVarListPtr->Name);
    FreePool (ConfigVarListPtr->Data);
  }

Done:
  if (ConfigVarListPtr != NULL) {
    FreePool (ConfigVarListPtr);
  }

  return Status;
}

/**
  Apply all settings from XML to their associated setting providers.

  @param[in] Buffer         Complete buffer of config settings in the format of XML.
  @param[in] Count          Number of bytes inside buffer, excluding NULL terminator.

  @retval EFI_SUCCESS       Settings are applied successfully.
  @retval other             Error occurred while attempting to apply supplied settings.
**/
EFI_STATUS
ApplySettings (
  IN  CHAR8  *Buffer,
  IN  UINTN  Count
  )
{
  XmlNode  *InputRootNode     = NULL;                     // The root xml node for the Input list.
  XmlNode  *InputPacketNode   = NULL;                     // The SettingsPacket node in the Input list
  XmlNode  *InputSettingsNode = NULL;                     // The Settings node for the Input list.
  XmlNode  *InputTempNode     = NULL;                     // Temp node ptr to use when moving thru the Input list

  XmlNode  *ResultRootNode     = NULL;                    // The root xml node in the result list
  XmlNode  *ResultPacketNode   = NULL;                    // The ResultsPacket node in the result list
  XmlNode  *ResultSettingsNode = NULL;                    // The Settings Node in the result list

  LIST_ENTRY  *Link = NULL;
  EFI_STATUS  Status;
  EFI_TIME    ApplyTime;
  UINTN       Version       = 0;
  UINTN       Lsv           = 0;
  BOOLEAN     ResetRequired = FALSE;

  UINTN       b64Size;
  UINTN       ValueSize;
  UINT8       *ByteArray;
  CONST VOID  *SetValue;

  ByteArray = NULL;
  SetValue  = NULL;

  //
  // Create Node List from input
  //
  Status = CreateXmlTree ((CHAR8 *)Buffer, Count, &InputRootNode);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a - Couldn't create a node list from the payload xml  %r\n", __func__, Status));
    Status = EFI_NO_MAPPING;
    goto EXIT;
  }

  // Print the list
  DEBUG ((DEBUG_INFO, "PRINTING INPUT XML - Start\n"));
  DebugPrintXmlTree (InputRootNode, 0);
  DEBUG ((DEBUG_INFO, "PRINTING INPUT XML - End\n"));

  //
  // Create Node List for output
  //
  Status = gRT->GetTime (&ApplyTime, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a - Failed to get time. %r\n", __func__, Status));
    Status = EFI_ABORTED;
    goto EXIT;
  }

  ResultRootNode = New_ResultPacketNodeList (&ApplyTime);
  if (ResultRootNode == NULL) {
    DEBUG ((DEBUG_ERROR, "%a - Couldn't create a node list from the result xml.\n", __func__));
    Status = EFI_ABORTED;
    goto EXIT;
  }

  // Get Input SettingsPacket Node
  InputPacketNode = GetSettingsPacketNode (InputRootNode);
  if (InputPacketNode == NULL) {
    DEBUG ((DEBUG_ERROR, "Failed to Get Input SettingsPacket Node\n"));
    Status = EFI_NO_MAPPING;
    goto EXIT;
  }

  // Get Output ResultsPacket Node
  ResultPacketNode = GetResultsPacketNode (ResultRootNode);
  if (ResultPacketNode == NULL) {
    DEBUG ((DEBUG_ERROR, "Failed to Get Output ResultsPacket Node\n"));
    Status = EFI_NO_MAPPING;
    goto EXIT;
  }

  //
  // Get input version
  //
  InputTempNode = FindFirstChildNodeByName (InputPacketNode, SETTINGS_VERSION_ELEMENT_NAME);
  if (InputTempNode == NULL) {
    DEBUG ((DEBUG_ERROR, "Failed to Get Version Node\n"));
    Status = EFI_NO_MAPPING;
    goto EXIT;
  }

  DEBUG ((DEBUG_INFO, "Incoming Version: %a\n", InputTempNode->Value));
  Version = AsciiStrDecimalToUintn (InputTempNode->Value);

  if (Version > 0xFFFFFFFF) {
    DEBUG ((DEBUG_ERROR, "Version Value invalid.  0x%x\n", Version));
    Status = EFI_NO_MAPPING;
    goto EXIT;
  }

  //
  // Get Incoming LSV
  //
  InputTempNode = FindFirstChildNodeByName (InputPacketNode, SETTINGS_LSV_ELEMENT_NAME);
  if (InputTempNode == NULL) {
    DEBUG ((DEBUG_ERROR, "Failed to Get LSV Node\n"));
    Status = EFI_NO_MAPPING;
    goto EXIT;
  }

  DEBUG ((DEBUG_INFO, "Incoming LSV: %a\n", InputTempNode->Value));
  Lsv = AsciiStrDecimalToUintn (InputTempNode->Value);

  if (Lsv > 0xFFFFFFFF) {
    DEBUG ((DEBUG_ERROR, "Lowest Supported Version Value invalid.  0x%x\n", Lsv));
    Status = EFI_NO_MAPPING;
    goto EXIT;
  }

  if (Lsv > Version) {
    DEBUG ((DEBUG_ERROR, "%a - LSV (%a) can't be larger than current version\n", __func__, InputTempNode->Value));
    Status = EFI_NO_MAPPING;
    goto EXIT;
  }

  // Get the Xml Node for the SettingsList
  InputSettingsNode = GetSettingsListNodeFromPacketNode (InputPacketNode);

  if (InputSettingsNode == NULL) {
    DEBUG ((DEBUG_ERROR, "Failed to Get Input Settings List Node\n"));
    Status = EFI_NO_MAPPING;
    goto EXIT;
  }

  ResultSettingsNode = GetSettingsListNodeFromPacketNode (ResultPacketNode);

  if (ResultSettingsNode == NULL) {
    DEBUG ((DEBUG_ERROR, "Failed to Get Result Settings List Node\n"));
    Status = EFI_ABORTED;  // internal xml..should never fail
    goto EXIT;
  }

  // All verified.   Now lets walk thru the Settings and try to apply each one.
  BASE_LIST_FOR_EACH (Link, &(InputSettingsNode->ChildrenListHead)) {
    XmlNode      *NodeThis = NULL;
    CONST CHAR8  *Id       = NULL;
    CONST CHAR8  *Value    = NULL;

    NodeThis = (XmlNode *)Link;   // Link is first member so just cast it.  this is the <Setting> node
    Status   = GetInputSettings (NodeThis, &Id, &Value);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to GetInputSettings.  Bad XML Data. %r\n", Status));
      Status = EFI_NO_MAPPING;
      goto EXIT;
    }

    // Now we have an Id and Value
    b64Size   = AsciiStrnLenS (Value, PcdGet32 (PcdMaxVariableSize));
    ValueSize = 0;
    Status    = Base64Decode (Value, b64Size, NULL, &ValueSize);
    if (Status != EFI_BUFFER_TOO_SMALL) {
      DEBUG ((DEBUG_ERROR, "Cannot query binary blob size. Code = %r\n", Status));
      Status = EFI_INVALID_PARAMETER;
      goto EXIT;
    }

    ByteArray = (UINT8 *)AllocatePool (ValueSize);

    Status = Base64Decode (Value, b64Size, ByteArray, &ValueSize);
    if (EFI_ERROR (Status)) {
      FreePool (ByteArray);
      DEBUG ((DEBUG_ERROR, "Cannot decode binary data. Code=%r\n", Status));
      Status = EFI_NO_MAPPING;
      goto EXIT;
    }

    SetValue = ByteArray;
    DEBUG ((DEBUG_INFO, "Setting BINARY data\n"));
    DUMP_HEX (DEBUG_VERBOSE, 0, SetValue, ValueSize, "");

    // Just write the variable
    Status = WriteSVDSetting (SetValue, ValueSize);

    DEBUG ((DEBUG_INFO, "%a - Set %a = %a. Result = %r\n", __func__, Id, Value, Status));

    // all done.
  } // end for loop

  // PRINT OUT XML HERE
  DEBUG ((DEBUG_INFO, "PRINTING OUT XML - Start\n"));
  DebugPrintXmlTree (ResultRootNode, 0);
  DEBUG ((DEBUG_INFO, "PRINTING OUTPUT XML - End\n"));

  Status = EFI_SUCCESS;

EXIT:
  if (InputRootNode) {
    FreeXmlTree (&InputRootNode);
  }

  if (ResultRootNode) {
    FreeXmlTree (&ResultRootNode);
  }

  if (NULL != ByteArray) {
    FreePool (ByteArray);
  }

  if (ResetRequired) {
    // Prepare Reset GUID
    ResetSystemWithSubtype (EfiResetCold, &gConfAppResetGuid);
  }

  return Status;
}

/**
 * Issue SvdUsbRequest - load settings from a USB drive
 *
 * @param  NONE
 *
 * @return -- This routine never returns to the caller
 */
STATIC
EFI_STATUS
ProcessSvdUsbInput (
  VOID
  )
{
  EFI_STATUS  Status;
  CHAR16      *FileName;
  CHAR8       *XmlString;
  UINTN       XmlStringSize;

  FileName  = NULL;
  XmlString = NULL;
  Status    = EFI_NOT_FOUND;

  //
  // Process request, from the PcdConfigurationFileName
  //
  FileName = AllocateCopyPool (PcdGetSize (PcdConfigurationFileName), PcdGetPtr (PcdConfigurationFileName));
  if (NULL != FileName) {
    Status = SvdRequestXmlFromUSB (
               FileName,
               &XmlString,
               &XmlStringSize
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Error loading backup file\n"));
      FreePool (FileName);
      FileName = NULL;
    }
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Error processing SVD Usb Request. Code=%r\n", Status));
  } else {
    DEBUG ((DEBUG_INFO, "SvdUsb Request processed normally\n"));
    // The XmlStringSize returned will be NULL terminated, thus trim off the tail.
    Status = ApplySettings (XmlString, XmlStringSize - 1);
    if (Status == EFI_MEDIA_CHANGED) {
      // MEDIA_CHANGED is a good return, It means that a JSON element updated a mailbox.
      Status = EFI_SUCCESS;
    }

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Error updating from JSON packet. Code=%r\n", __func__, Status));
    }
  }

  if (NULL != XmlString) {
    FreePool (XmlString);
  }

  if (!EFI_ERROR (Status)) {
    //
    // Inform user that operation is complete
    //
    Print (L"Applied %s for configuration update. Rebooting now!!!\n", FileName);

    // Prepare Reset GUID
    ResetSystemWithSubtype (EfiResetCold, &gConfAppResetGuid);
    // Should not be here
    CpuDeadLoop ();
  }

  if (NULL != FileName) {
    // If we ever return, free the allocated buffer.
    FreePool (FileName);
  }

  return Status;
}

/**
 * Issue SvdSerialRequest - load settings from a USB drive
 *
 * @param  NONE
 *
 * @return -- This routine never returns to the caller
 */
STATIC
EFI_STATUS
ProcessSvdSerialInput (
  CHAR16  UnicodeChar
  )
{
  EFI_STATUS  Status;
  CHAR8       *TempAsciiStr;

  Status       = EFI_SUCCESS;
  TempAsciiStr = NULL;

  // Simple resizable array and store them at mConfDataBuffer
  if (mConfDataBuffer == NULL) {
    mConfDataSize   = EFI_PAGE_SIZE;
    mConfDataBuffer = AllocateZeroPool (mConfDataSize * sizeof (CHAR16));
    mConfDataOffset = 0;
  } else if (mConfDataOffset >= (mConfDataSize / 2)) {
    mConfDataBuffer = ReallocatePool (mConfDataSize * sizeof (CHAR16), mConfDataSize * 2 * sizeof (CHAR16), mConfDataBuffer);
    mConfDataSize   = mConfDataSize * 2;
    if (mConfDataSize <= mConfDataOffset) {
      DEBUG ((
        DEBUG_ERROR,
        "%a Configuration data offset (0x%x) is out of boundary (0x%x)\n",
        __func__,
        mConfDataOffset,
        mConfDataSize
        ));
      ASSERT (FALSE);
      Status = EFI_BAD_BUFFER_SIZE;
      goto Exit;
    }
  }

  if ((UnicodeChar == CHAR_CARRIAGE_RETURN) || (UnicodeChar == CHAR_LINEFEED)) {
    mConfDataBuffer[mConfDataOffset++] = '\0';
    TempAsciiStr                       = AllocateZeroPool (mConfDataOffset);
    Status                             = UnicodeStrToAsciiStrS (mConfDataBuffer, TempAsciiStr, mConfDataOffset);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a Failed to convert received data to Ascii string - %r\n", __func__, Status));
      ASSERT_EFI_ERROR (Status);
      goto Exit;
    }

    Status = ApplySettings (TempAsciiStr, mConfDataOffset - 1);
    if (Status == EFI_NO_MAPPING) {
      // When failing to parse incoming buffer, we give users another chance, after cleaning up the buffer.
      DEBUG ((DEBUG_ERROR, "%a Failed to parse SVD file.\n", __func__));
      FreePool (mConfDataBuffer);
      mConfDataBuffer = NULL;
      goto Exit;
    } else if (EFI_ERROR (Status)) {
      // For other failures, we either assert or reboot.
      DEBUG ((DEBUG_ERROR, "%a Failed to apply received settings - %r\n", __func__, Status));
      ASSERT_EFI_ERROR (Status);
      goto Exit;
    }

    // Prepare Reset GUID
    ResetSystemWithSubtype (EfiResetCold, &gConfAppResetGuid);
    // Should not be here
    CpuDeadLoop ();
  } else {
    // Keep listening, if an odd char comes in, it will be a terminator and cause the parser to fail anyway.
    Print (L"%c", UnicodeChar);
    mConfDataBuffer[mConfDataOffset++] = UnicodeChar;
  }

Exit:
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a Failed to process unicode from keystroke - %r\n", __func__, Status));
  }

  return Status;
}

/**
Create an XML string from all the current settings

**/
EFI_STATUS
EFIAPI
CreateXmlStringFromCurrentSettings (
  OUT CHAR8  **XmlString,
  OUT UINTN  *StringSize
  )
{
  EFI_STATUS             Status;
  XmlNode                *List                    = NULL;
  XmlNode                *CurrentSettingsNode     = NULL;
  XmlNode                *CurrentSettingsListNode = NULL;
  CHAR8                  LsvString[20];
  EFI_TIME               Time;
  UINT32                 Lsv            = 1;
  UINTN                  DataSize       = 0;
  UINTN                  VarListSize    = 0;
  UINTN                  Offset         = 0;
  CHAR8                  *EncodedBuffer = NULL;
  UINTN                  EncodedSize    = 0;
  VOID                   *Data          = NULL;
  CHAR8                  *AsciiName     = NULL;
  UINTN                  AsciiSize;
  UINTN                  i;
  UINTN                  NumPolicies;
  EFI_GUID               *TargetGuids;
  CONFIG_VAR_LIST_ENTRY  ConfigVarList;

  if ((XmlString == NULL) || (StringSize == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  PERF_FUNCTION_BEGIN ();

  // Clear any uncertainty.
  ZeroMem (&ConfigVarList, sizeof (CONFIG_VAR_LIST_ENTRY));

  // create basic xml
  Status = gRT->GetTime (&Time, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a - Failed to get time. %r\n", __func__, Status));
    goto EXIT;
  }

  List = New_CurrentSettingsPacketNodeList (&Time);
  if (List == NULL) {
    DEBUG ((DEBUG_ERROR, "%a - Failed to create new Current Settings Packet List Node\n", __func__));
    Status = EFI_ABORTED;
    goto EXIT;
  }

  // Get SettingsPacket Node
  CurrentSettingsNode = GetCurrentSettingsPacketNode (List);
  if (CurrentSettingsNode == NULL) {
    DEBUG ((DEBUG_ERROR, "Failed to Get GetCurrentSettingsPacketNode Node\n"));
    Status = EFI_NO_MAPPING;
    goto EXIT;
  }

  // Record Status result
  //
  // Add the Lowest Supported Version Node
  //
  ZeroMem (LsvString, sizeof (LsvString));
  AsciiValueToStringS (&(LsvString[0]), sizeof (LsvString), 0, (UINT32)Lsv, 19);
  Status = AddSettingsLsvNode (CurrentSettingsNode, LsvString);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to set LSV Node for current settings. %r\n", Status));
    goto EXIT;
  }

  //
  // Get the Settings Node List Node
  //
  CurrentSettingsListNode = GetSettingsListNodeFromPacketNode (CurrentSettingsNode);
  if (CurrentSettingsListNode == NULL) {
    DEBUG ((DEBUG_ERROR, "Failed to Get Settings List Node from Packet Node\n"));
    Status = EFI_NO_MAPPING;
    goto EXIT;
  }

  // Need to fit in a GUID : %08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x
  AsciiSize = sizeof (EFI_GUID) * 2 + 4 + 1;
  AsciiName = AllocateZeroPool (AsciiSize);
  if (AsciiName == NULL) {
    DEBUG ((DEBUG_ERROR, "Failed to allocate buffer for initial variable name.\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  // Inspect the size of PCD first.
  NumPolicies = PcdGetSize (PcdConfigurationPolicyGuid);

  // if we get a bad size for the configuration GUID list, we just fail...
  if ((NumPolicies == 0) || (NumPolicies % sizeof (EFI_GUID) != 0)) {
    DEBUG ((DEBUG_ERROR, "%a Invalid number of bytes in PcdConfigurationPolicyGuid: %u!\n", __func__, NumPolicies));
    ASSERT (FALSE);
  } else {
    NumPolicies /= sizeof (EFI_GUID);

    TargetGuids = (EFI_GUID *)PcdGetPtr (PcdConfigurationPolicyGuid);

    // if the PCD is not specified, there is nothing we can dump here...
    if (TargetGuids == NULL) {
      DEBUG ((DEBUG_ERROR, "%a Failed to get list of valid GUIDs!\n", __func__));
      ASSERT (FALSE);
    } else {
      // Try to locate all config policies specified in this PCD
      for (i = 0; i < NumPolicies; i++) {
        DataSize = 0;
        Status   = mPolicyProtocol->GetPolicy (&TargetGuids[i], NULL, Data, (UINT16 *)&DataSize);
        if (Status != EFI_BUFFER_TOO_SMALL) {
          DEBUG ((DEBUG_ERROR, "%a Failed to get configuration policy size %g - %r\n", __func__, &TargetGuids[i], Status));
          ASSERT (FALSE);
          continue;
        }

        Data = AllocatePool (DataSize);
        if (Data == NULL) {
          DEBUG ((DEBUG_ERROR, "%a Unable to allocate pool for configuration policy %g\n", __func__, &TargetGuids[i]));
          break;
        }

        Status = mPolicyProtocol->GetPolicy (&TargetGuids[i], NULL, Data, (UINT16 *)&DataSize);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a Failed to get configuration policy %g - %r\n", __func__, &TargetGuids[i], Status));
          ASSERT (FALSE);
          FreePool (Data);
          Data = NULL;
          continue;
        }

        Offset = 0;
        while (Offset < DataSize) {
          VarListSize = DataSize - Offset;
          Status      = ConvertVariableListToVariableEntry ((UINT8 *)Data + Offset, &VarListSize, &ConfigVarList);
          if (EFI_ERROR (Status)) {
            DEBUG ((DEBUG_ERROR, "%a Failed to convert variable list to variable entry - %r\n", __func__, Status));
            goto EXIT;
          }

          AsciiSize = StrnSizeS (ConfigVarList.Name, CONF_VAR_NAME_LEN) / sizeof (CHAR16);
          AsciiName = AllocatePool (AsciiSize);
          if (AsciiName == NULL) {
            DEBUG ((DEBUG_ERROR, "Failed to allocate buffer for ID %s.\n", ConfigVarList.Name));
            Status = EFI_OUT_OF_RESOURCES;
            goto EXIT;
          }

          AsciiSPrint (AsciiName, AsciiSize, "%s", ConfigVarList.Name);

          // First encode the binary blob
          EncodedSize = 0;
          Status      = Base64Encode ((UINT8 *)Data + Offset, VarListSize, NULL, &EncodedSize);
          if (Status != EFI_BUFFER_TOO_SMALL) {
            DEBUG ((DEBUG_ERROR, "Cannot query binary blob size. Code = %r\n", Status));
            Status = EFI_INVALID_PARAMETER;
            goto EXIT;
          }

          EncodedBuffer = (CHAR8 *)AllocatePool (EncodedSize);
          if (EncodedBuffer == NULL) {
            DEBUG ((DEBUG_ERROR, "Cannot allocate encoded buffer of size 0x%x.\n", EncodedSize));
            Status = EFI_OUT_OF_RESOURCES;
            goto EXIT;
          }

          Status = Base64Encode ((UINT8 *)Data + Offset, VarListSize, EncodedBuffer, &EncodedSize);
          if (EFI_ERROR (Status)) {
            DEBUG ((DEBUG_ERROR, "Failed to encode binary data into Base 64 format. Code = %r\n", Status));
            Status = EFI_INVALID_PARAMETER;
            goto EXIT;
          }

          Status = SetCurrentSettings (CurrentSettingsListNode, AsciiName, EncodedBuffer);

          if (EFI_ERROR (Status)) {
            DEBUG ((DEBUG_ERROR, "%a - Error from Set Current Settings.  Status = %r\n", __func__, Status));
          }

          Offset += VarListSize;
          FreePool (ConfigVarList.Name);
          FreePool (ConfigVarList.Data);
          FreePool (AsciiName);
          ConfigVarList.Data = NULL;
          ConfigVarList.Name = NULL;
          AsciiName          = NULL;
        }

        FreePool (Data);
        Data = NULL;
      }
    }
  }

  // now output as xml string
  Status = XmlTreeToString (List, TRUE, StringSize, XmlString);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a - XmlTreeToString failed.  %r\n", __func__, Status));
  }

EXIT:
  if (List != NULL) {
    FreeXmlTree (&List);
  }

  if (Data != NULL) {
    FreePool (Data);
  }

  if (ConfigVarList.Name != NULL) {
    FreePool (ConfigVarList.Name);
  }

  if (ConfigVarList.Data != NULL) {
    FreePool (ConfigVarList.Data);
  }

  if (EncodedBuffer != NULL) {
    FreePool (EncodedBuffer);
  }

  if (AsciiName != NULL) {
    FreePool (AsciiName);
  }

  if (EFI_ERROR (Status)) {
    // free memory since it was an error
    if (*XmlString != NULL) {
      FreePool (*XmlString);
      *XmlString  = NULL;
      *StringSize = 0;
    }
  }

  PERF_FUNCTION_END ();
  return Status;
}

/**
  State machine for configuration setup. It will react to user keystroke to accept
  configuration data from selected option.

  @retval EFI_SUCCESS           This iteration of state machine proceeds successfully.
  @retval Others                Failed to wait for valid keystrokes or application of
                                new configuration data failed.
**/
EFI_STATUS
EFIAPI
SetupConfMgr (
  VOID
  )
{
  EFI_STATUS    Status = EFI_SUCCESS;
  EFI_KEY_DATA  KeyData;
  CHAR8         *StrBuf = NULL;
  UINTN         StrBufSize;
  UINTN         Index;
  UINTN         PrintedUnicode;

  switch (mSetupConfState) {
    case SetupConfInit:
      if (mPolicyProtocol == NULL) {
        Status = gBS->LocateProtocol (
                        &gPolicyProtocolGuid,
                        NULL,
                        (VOID **)&mPolicyProtocol
                        );
      }

      // Collect what is needed and print in this step
      Status = PrintOptions ();
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a Error occurred while printing configuration options - %r\n", __func__, Status));
        ASSERT (FALSE);
        break;
      }

      mSetupConfState = SetupConfWait;
      break;
    case SetupConfWait:
      // Wait for key stroke event.
      //
      Status = PollKeyStroke (FALSE, 0, &KeyData);
      if (Status == EFI_NOT_READY) {
        // Do nothing
      } else if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a Error occurred while waiting for configuration selection - %r\n", __func__, Status));
        ASSERT (FALSE);
      } else {
        Status = CheckSupportedOptions (&KeyData, SetupConfStateOptions, SETUP_CONF_STATE_OPTIONS, (UINT32 *)&mSetupConfState);
        if (Status == EFI_NOT_FOUND) {
          Status = EFI_SUCCESS;
        } else if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a Error processing incoming keystroke - %r\n", __func__, Status));
          ASSERT (FALSE);
        }
      }

      break;
    case SetupConfUpdateUsb:
      // Locate data blob from local USB device and apply changes. This function does not return
      //
      Status = ProcessSvdUsbInput ();
      if (Status == EFI_NOT_FOUND) {
        // Do not assert if we just cannot find the file...
        Print (L"\nCould not find USB file %s\n", PcdGetPtr (PcdConfigurationFileName));
        Status          = EFI_SUCCESS;
        mSetupConfState = SetupConfWait;
        break;
      } else if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a Failed to load configuration data from USB - %r\n", __func__, Status));
        ASSERT (FALSE);
      }

      mSetupConfState = SetupConfExit;
      break;
    case SetupConfUpdateSerialHint:
      Print (L"\nPaste or send the formatted configuration payload here:\n");
      mSetupConfState = SetupConfUpdateSerial;
    case SetupConfUpdateSerial:
      // Wait for incoming unicode chars. This is performance sensitive, thus invoking the protocol function directly.
      //
      Status = mSimpleTextInEx->ReadKeyStrokeEx (mSimpleTextInEx, &KeyData);
      if (Status == EFI_NOT_READY) {
        // No key pressed, mask the error
        Status = EFI_SUCCESS;
        break;
      } else if (EFI_ERROR (Status)) {
        break;
      }

      if (KeyData.Key.ScanCode == SCAN_ESC) {
        mSetupConfState = SetupConfExit;
      } else {
        Status = ProcessSvdSerialInput (KeyData.Key.UnicodeChar);
        if (Status == EFI_NO_MAPPING) {
          // Do not fail completely if we just cannot find the file...
          gST->ConOut->SetAttribute (gST->ConOut, EFI_TEXT_ATTR (EFI_YELLOW, EFI_BLACK));
          Print (L"\nFailed to parse input SVD data, please check the input and try again.\n");
          gST->ConOut->SetAttribute (gST->ConOut, EFI_TEXT_ATTR (EFI_WHITE, EFI_BLACK));
          Status          = EFI_SUCCESS;
          mSetupConfState = SetupConfUpdateSerialHint;
          break;
        } else if (EFI_ERROR (Status)) {
          mSetupConfState = SetupConfExit;
        }
      }

      break;
    case SetupConfDumpSerial:
      // Clear screen
      PrintScreenInit ();
      Status = CreateXmlStringFromCurrentSettings (&StrBuf, &StrBufSize);
      if (EFI_ERROR (Status)) {
        Print (L"\nFailed to print current settings in SVD format - %r\n", Status);
        Status = EFI_SUCCESS;
      } else {
        Status = InspectDumpOutput (StrBuf, StrBufSize);
        if (EFI_ERROR (Status)) {
          Print (L"\nGenerated print failed to pass inspection - %r\n", Status);
          Status = EFI_SUCCESS;
          break;
        }

        Print (L"\nCurrent configurations are dumped Below in format of *.SVD:\n");
        Print (L"\n");
        Index = 0;
        while (Index < StrBufSize) {
          PrintedUnicode = Print (L"%a", &StrBuf[Index]);
          Index         += PrintedUnicode;
          if (PrintedUnicode == 0) {
            // So that we will not get stuck if Print function malfunctions
            break;
          }
        }

        Print (L"\n");
      }

      mSetupConfState = SetupConfDumpComplete;
      break;
    case SetupConfDumpComplete:
      // Print hint on how to get out of here..
      Print (L"\nPress 'ESC' to return to Setup menu.\n");
      // Wait for key stroke event.
      //
      Status = PollKeyStroke (FALSE, 0, &KeyData);
      if (Status == EFI_NOT_READY) {
        // Do nothing
      } else if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a Error occurred while waiting for configuration selection - %r\n", __func__, Status));
        ASSERT (FALSE);
      } else if ((KeyData.Key.UnicodeChar == CHAR_NULL) && (KeyData.Key.ScanCode == SCAN_ESC)) {
        mSetupConfState = SetupConfInit;
      }

      break;
    case SetupConfError:
      Print (L"Cannot change configurations at current mode!\n");
      mSetupConfState = SetupConfWait;
      break;
    case SetupConfExit:
      ResetGlobals ();
      ExitSubRoutine ();
      break;
    default:
      ASSERT (FALSE);
      Status = EFI_DEVICE_ERROR;
      break;
  }

  return Status;
}
