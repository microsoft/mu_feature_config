/** @file
  Test driver to locate conf data from variable storage and print all contained entries.

  Copyright (c) 2017 - 2020, Intel Corporation. All rights reserved.<BR>
  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <XmlTypes.h>
#include <Guid/DfciPacketHeader.h>

#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/ResetSystemLib.h>
#include <Library/DfciUiSupportLib.h>
#include <Library/ConfigDataLib.h>
#include <Library/XmlTreeLib.h>
#include <Library/XmlTreeQueryLib.h>
#include <Library/DfciXmlSettingSchemaSupportLib.h>
#include <Library/PerformanceLib.h>
#include <Library/ConfigVariableListLib.h>

#include "ConfApp.h"
#include "DfciUsb/DfciUsb.h"

#define DEFAULT_USB_FILE_NAME  L"SetupConfUpdate.svd"
#define CURRENT_XML_TEMPLATE   "<?xml version=\"1.0\" encoding=\"utf-8\"?><CurrentSettingsPacket xmlns=\"urn:UefiSettings-Schema\"></CurrentSettingsPacket>"

#define SETUP_CONF_STATE_OPTIONS  6

CONST ConfAppKeyOptions  SetupConfStateOptions[SETUP_CONF_STATE_OPTIONS] = {
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
    .Description         = L"Update from Network.",
    .DescriptionTextAttr = EFI_TEXT_ATTR (EFI_WHITE, EFI_BLACK),
    .UnicodeChar         = '2',
    .ScanCode            = SCAN_NULL,
    .EndState            = SetupConfUpdateNetwork
  },
  {
    .KeyName             = L"3",
    .KeyNameTextAttr     = EFI_TEXT_ATTR (EFI_YELLOW, EFI_BLACK),
    .Description         = L"Update from Serial Port.",
    .DescriptionTextAttr = EFI_TEXT_ATTR (EFI_WHITE, EFI_BLACK),
    .UnicodeChar         = '3',
    .ScanCode            = SCAN_NULL,
    .EndState            = SetupConfUpdateSerialHint
  },
  {
    .KeyName             = L"4",
    .KeyNameTextAttr     = EFI_TEXT_ATTR (EFI_YELLOW, EFI_BLACK),
    .Description         = L"Dump Current Configuration.\n",
    .DescriptionTextAttr = EFI_TEXT_ATTR (EFI_WHITE, EFI_BLACK),
    .UnicodeChar         = '4',
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

  Status = PrintAvailableOptions (SetupConfStateOptions, SETUP_CONF_STATE_OPTIONS);
  if (EFI_ERROR (Status)) {
    ASSERT (FALSE);
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

  LIST_ENTRY          *Link = NULL;
  EFI_STATUS          Status;
  DFCI_SETTING_FLAGS  Flags = 0;
  EFI_TIME            ApplyTime;
  UINTN               Version       = 0;
  UINTN               Lsv           = 0;
  BOOLEAN             ResetRequired = FALSE;

  UINTN       b64Size;
  UINTN       ValueSize;
  UINT8       *ByteArray = NULL;
  CONST VOID  *SetValue  = NULL;

  CONFIG_VAR_LIST_ENTRY  VarListEntry;

  //
  // Create Node List from input
  //
  Status = CreateXmlTree ((CHAR8 *)Buffer, Count, &InputRootNode);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a - Couldn't create a node list from the payload xml  %r\n", __FUNCTION__, Status));
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
    DEBUG ((DEBUG_ERROR, "%a - Failed to get time. %r\n", __FUNCTION__, Status));
    Status = EFI_ABORTED;
    goto EXIT;
  }

  ResultRootNode = New_ResultPacketNodeList (&ApplyTime);
  if (ResultRootNode == NULL) {
    DEBUG ((DEBUG_ERROR, "%a - Couldn't create a node list from the result xml.\n", __FUNCTION__));
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

  // check against lsv
  // if (InternalData->LSV > (UINT32)Version)
  // {
  //   DEBUG ((DEBUG_ERROR, "Setting Version Less Than System LSV(%ld)\n", InternalData->LSV));
  //   Status = EFI_ACCESS_DENIED;
  //   goto EXIT;
  // }

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
    DEBUG ((DEBUG_ERROR, "%a - LSV (%a) can't be larger than current version\n", __FUNCTION__, InputTempNode->Value));
    Status = EFI_NO_MAPPING;
    goto EXIT;
  }

  // set the new version
  // if (InternalData->CurrentVersion != (UINT32)Version)
  // {
  //   InternalData->CurrentVersion = (UINT32)Version;
  //   InternalData->Modified = TRUE;
  // }

  // If new LSV is larger set it
  // if ((UINT32)Lsv > InternalData->LSV)
  // {
  //   DEBUG ((DEBUG_ERROR, "%a - Setting New LSV (0x%X)\n", __FUNCTION__, Lsv));
  //   InternalData->LSV = (UINT32)Lsv;
  //   InternalData->Modified = TRUE;
  // }

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
    XmlNode                 *NodeThis = NULL;
    DFCI_SETTING_ID_STRING  Id        = NULL;
    CONST CHAR8             *Value    = NULL;
    CHAR8                   StatusString[25]; // 0xFFFFFFFFFFFFFFFF\n
    CHAR8                   FlagString[25];

    Flags = 0;

    NodeThis = (XmlNode *)Link;   // Link is first member so just cast it.  this is the <Setting> node
    Status   = GetInputSettings (NodeThis, &Id, &Value);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to GetInputSettings.  Bad XML Data. %r\n", Status));
      Status = EFI_NO_MAPPING;
      goto EXIT;
    }

    // only care about our target
    ZeroMem (&VarListEntry, sizeof (VarListEntry));
    Status = QuerySingleActiveConfigAsciiVarList (Id, &VarListEntry);
    if (EFI_ERROR (Status)) {
      continue;
    }

    // Now we have an Id and Value
    b64Size   = AsciiStrnLenS (Value, MAX_ALLOWABLE_DFCI_APPLY_VAR_SIZE);
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
      DEBUG ((DEBUG_ERROR, "Cannot set binary data. Code=%r\n", Status));
      goto EXIT;
    }

    SetValue = ByteArray;
    DEBUG ((DEBUG_INFO, "Setting BINARY data\n"));
    DUMP_HEX (DEBUG_VERBOSE, 0, SetValue, ValueSize, "");

    if (mSettingAccess == NULL) {
      // Should not be here
      Status = EFI_NOT_STARTED;
      goto EXIT;
    }

    // Now set the settings
    Status = mSettingAccess->Set (
                               mSettingAccess,
                               Id,
                               &mAuthToken,
                               DFCI_SETTING_TYPE_BINARY,
                               ValueSize,
                               SetValue,
                               &Flags
                               );
    DEBUG ((DEBUG_INFO, "%a - Set %a = %a. Result = %r\n", __FUNCTION__, Id, Value, Status));

    // Record Status result
    ZeroMem (StatusString, sizeof (StatusString));
    ZeroMem (FlagString, sizeof (FlagString));
    StatusString[0] = '0';
    StatusString[1] = 'x';
    FlagString[0]   = '0';
    FlagString[1]   = 'x';

    AsciiValueToStringS (&(StatusString[2]), sizeof (StatusString)-2, RADIX_HEX, (INT64)Status, 18);
    AsciiValueToStringS (&(FlagString[2]), sizeof (FlagString)-2, RADIX_HEX, (INT64)Flags, 18);
    Status = SetOutputSettingsStatus (ResultSettingsNode, Id, &(StatusString[0]), &(FlagString[0]));
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to SetOutputSettingStatus.  %r\n", Status));
      Status = EFI_DEVICE_ERROR;
      goto EXIT;
    }

    if (Flags & DFCI_SETTING_FLAGS_OUT_REBOOT_REQUIRED) {
      ResetRequired = TRUE;
    }

    // all done.
  } // end for loop

  // PRINT OUT XML HERE
  DEBUG ((DEBUG_INFO, "PRINTING OUT XML - Start\n"));
  DebugPrintXmlTree (ResultRootNode, 0);
  DEBUG ((DEBUG_INFO, "PRINTING OUTPUT XML - End\n"));

  // // convert result xml node list to string
  // Status = XmlTreeToString (ResultRootNode, TRUE, &ResultXmlSize, &Data->ResultXml);
  // if (EFI_ERROR (Status))
  // {
  //   DEBUG ((DEBUG_ERROR, "Failed to convert Result XML to String.  Status = %r\n", Status));
  //   Status = EFI_ABORTED;
  //   goto EXIT;
  // }

  // // make sure its a good size
  // if (Data->ResultXmlSize > MAX_ALLOWABLE_DFCI_RESULT_VAR_SIZE)
  // {
  //   DEBUG ((DEBUG_ERROR, "Size of result XML doc is too large (0x%X).\n", Data->ResultXmlSize));
  //   Status = EFI_ABORTED;
  //   goto EXIT;
  // }

  // StrLen = AsciiStrSize (Data->ResultXml);
  // if (Data->ResultXmlSize != StrLen)
  // {
  //   DEBUG ((DEBUG_ERROR, "ResultXmlSize is not the correct size\n"));
  // }
  // DEBUG ((DEBUG_INFO, "%a - ResultXmlSize = 0x%X  ResultXml String Length = 0x%X\n", __FUNCTION__, Data->ResultXmlSize, StrLen));
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
    ResetCold ();
  }

  return Status;
}

/**
 * Issue DfciUsbRequest - load settings from a USB drive
 *
 * @param  NONE
 *
 * @return -- This routine never returns to the caller
 */
STATIC
EFI_STATUS
ProcessDfciUsbInput (
  VOID
  )
{
  EFI_STATUS  Status;
  CHAR16      *FileName;
  CHAR16      *FileName2;
  CHAR8       *XmlString;
  UINTN       XmlStringSize;

  DfciUiExitSecurityBoundary ();

  FileName  = NULL;
  XmlString = NULL;

  //
  // Process request
  //
  Status = BuildUsbRequest (L".svd", &FileName);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Error building Usb Request. Code=%r\n", Status));
  } else {
    Status = DfciRequestJsonFromUSB (
               FileName,
               &XmlString,
               &XmlStringSize
               );
    if (EFI_ERROR (Status)) {
      FileName2 = AllocateCopyPool (sizeof (DEFAULT_USB_FILE_NAME), DEFAULT_USB_FILE_NAME);
      if (NULL != FileName2) {
        Status = DfciRequestJsonFromUSB (
                   FileName2,
                   &XmlString,
                   &XmlStringSize
                   );
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "Error loading backup file\n"));
          FreePool (FileName2);
        } else {
          FreePool (FileName);
          FileName = FileName2;
        }
      }
    }

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Error processing Dfci Usb Request. Code=%r\n", Status));
    } else {
      DEBUG ((DEBUG_INFO, "DfciUsb Request processed normally\n"));
      Status = ApplySettings (XmlString, XmlStringSize);
      if (Status == EFI_MEDIA_CHANGED) {
        // MEDIA_CHANGED is a good return, It means that a JSON element updated a mailbox.
        Status = EFI_SUCCESS;
      }

      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Error updating from JSON packet. Code=%r\n", __FUNCTION__, Status));
      }
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

    ResetCold ();
    // Should not be here
    CpuDeadLoop ();
  }

  return Status;
}

/**
 * Issue DfciSerialRequest - load settings from a USB drive
 *
 * @param  NONE
 *
 * @return -- This routine never returns to the caller
 */
STATIC
EFI_STATUS
ProcessDfciSerialInput (
  CHAR16  UnicodeChar
  )
{
  EFI_STATUS  Status        = EFI_SUCCESS;
  CHAR8       *TempAsciiStr = NULL;

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
        __FUNCTION__,
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
      DEBUG ((DEBUG_ERROR, "%a Failed to convert received data to Ascii string - %r\n", __FUNCTION__, Status));
      ASSERT_EFI_ERROR (Status);
      goto Exit;
    }

    Status = ApplySettings (TempAsciiStr, mConfDataOffset - 1);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a Failed to apply received settings - %r\n", __FUNCTION__, Status));
      ASSERT_EFI_ERROR (Status);
      goto Exit;
    }

    ResetCold ();
    // Should not be here
    CpuDeadLoop ();
  } else {
    // Keep listening, if an odd char comes in, it will be a terminator and cause the parser to fail anyway.
    Print (L"%c", UnicodeChar);
    mConfDataBuffer[mConfDataOffset++] = UnicodeChar;
  }

Exit:
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a Failed to process unicode from keystroke - %r\n", __FUNCTION__, Status));
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
  UINT32                 Lsv      = 1;
  UINTN                  DataSize = 0;
  CHAR8                  *EncodedBuffer;
  UINTN                  EncodedSize = 0;
  VOID                   *Data       = NULL;
  UINT8                  Dummy;
  DFCI_SETTING_FLAGS     Flags;
  CHAR8                  *AsciiName;
  UINTN                  AsciiSize;
  CONFIG_VAR_LIST_ENTRY  *VarListEntries     = NULL;
  UINTN                  VarListEntriesCount = 0;
  UINTN                  Index;

  if ((XmlString == NULL) || (StringSize == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  PERF_FUNCTION_BEGIN ();

  // create basic xml
  Status = gRT->GetTime (&Time, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a - Failed to get time. %r\n", __FUNCTION__, Status));
    goto EXIT;
  }

  List = New_CurrentSettingsPacketNodeList (&Time);
  if (List == NULL) {
    DEBUG ((DEBUG_ERROR, "%a - Failed to create new Current Settings Packet List Node\n", __FUNCTION__));
    Status = EFI_ABORTED;
    goto EXIT;
  }

  // Get SettingsPacket Node
  CurrentSettingsNode = GetCurrentSettingsPacketNode (List);
  if (CurrentSettingsNode == NULL) {
    DEBUG ((DEBUG_INFO, "Failed to Get GetCurrentSettingsPacketNode Node\n"));
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
    DEBUG ((DEBUG_INFO, "Failed to set LSV Node for current settings. %r\n", Status));
    goto EXIT;
  }

  //
  // Get the Settings Node List Node
  //
  CurrentSettingsListNode = GetSettingsListNodeFromPacketNode (CurrentSettingsNode);
  if (CurrentSettingsListNode == NULL) {
    DEBUG ((DEBUG_INFO, "Failed to Get Settings List Node from Packet Node\n"));
    Status = EFI_NO_MAPPING;
    goto EXIT;
  }

  Status = RetrieveActiveConfigVarList (&VarListEntries, &VarListEntriesCount);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Retrieving config data failed - %r\n", Status));
    goto EXIT;
  }

  if ((VarListEntries == NULL) && (VarListEntriesCount != 0)) {
    // Should not be here, but...
    DEBUG ((DEBUG_ERROR, "Retrieved config data is NULL.\n"));
    Status = EFI_COMPROMISED_DATA;
    goto EXIT;
  }

  // Now get the individual settings
  for (Index = 0; Index < VarListEntriesCount; Index++) {
    if (VarListEntries[Index].Name == NULL) {
      DEBUG ((DEBUG_ERROR, "Retrieved config data index 0x%x has NULL name.\n", Index));
      Status = EFI_COMPROMISED_DATA;
      goto EXIT;
    }

    AsciiSize = StrnLenS (VarListEntries[Index].Name, DFCI_MAX_ID_LEN) + 1;
    AsciiName = AllocatePool (AsciiSize);
    if (AsciiName == NULL) {
      DEBUG ((DEBUG_ERROR, "Failed to allocate buffer for ID %s.\n", VarListEntries[Index].Name));
      Status = EFI_OUT_OF_RESOURCES;
      goto EXIT;
    }

    AsciiSPrint (AsciiName, AsciiSize, "%s", VarListEntries[Index].Name);

    DataSize = 0;
    Status   = mSettingAccess->Get (
                                 mSettingAccess,
                                 AsciiName,
                                 &mAuthToken,
                                 DFCI_SETTING_TYPE_BINARY,
                                 &DataSize,
                                 &Dummy,
                                 &Flags
                                 );
    if (Status != EFI_BUFFER_TOO_SMALL) {
      DEBUG ((DEBUG_ERROR, "%a - Get binary configuration size returned unexpected result = %r\n", __FUNCTION__, Status));
      goto EXIT;
    }

    Data = AllocatePool (DataSize);
    if (Data == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto EXIT;
    }

    Status = mSettingAccess->Get (
                               mSettingAccess,
                               AsciiName,
                               &mAuthToken,
                               DFCI_SETTING_TYPE_BINARY,
                               &DataSize,
                               Data,
                               &Flags
                               );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a - Get binary configuration data returned unexpected result = %r\n", __FUNCTION__, Status));
      goto EXIT;
    }

    // First encode the binary blob
    EncodedSize = 0;
    Status      = Base64Encode (Data, DataSize, NULL, &EncodedSize);
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

    Status = Base64Encode (Data, DataSize, EncodedBuffer, &EncodedSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to encode binary data into Base 64 format. Code = %r\n", Status));
      return EFI_INVALID_PARAMETER;
    }

    Status = SetCurrentSettings (CurrentSettingsListNode, AsciiName, EncodedBuffer);

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a - Error from Set Current Settings.  Status = %r\n", __FUNCTION__, Status));
    }

    FreePool (Data);
    Data = NULL;
  }

  // now output as xml string
  Status = XmlTreeToString (List, TRUE, StringSize, XmlString);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a - XmlTreeToString failed.  %r\n", __FUNCTION__, Status));
  }

EXIT:
  if (List != NULL) {
    FreeXmlTree (&List);
  }

  if (Data != NULL) {
    FreePool (Data);
  }

  if (EncodedBuffer != NULL) {
    FreePool (EncodedBuffer);
  }

  if (VarListEntries != NULL) {
    for (Index = 0; Index < VarListEntriesCount; Index++) {
      if (VarListEntries[Index].Name != NULL) {
        FreePool (VarListEntries[Index].Name);
      }

      if (VarListEntries[Index].Data != NULL) {
        FreePool (VarListEntries[Index].Data);
      }
    }

    FreePool (VarListEntries);
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
  CHAR8         *StrBuf;
  UINTN         StrBufSize;

  switch (mSetupConfState) {
    case SetupConfInit:
      // Collect what is needed and print in this step
      Status = PrintOptions ();
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a Error occurred while printing configuration options - %r\n", __FUNCTION__, Status));
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
        DEBUG ((DEBUG_ERROR, "%a Error occurred while waiting for configuration selection - %r\n", __FUNCTION__, Status));
        ASSERT (FALSE);
      } else {
        Status = CheckSupportedOptions (&KeyData, SetupConfStateOptions, SETUP_CONF_STATE_OPTIONS, (UINT32 *)&mSetupConfState);
        if (Status == EFI_NOT_FOUND) {
          Status = EFI_SUCCESS;
        } else if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a Error processing incoming keystroke - %r\n", __FUNCTION__, Status));
          ASSERT (FALSE);
        }
      }

      break;
    case SetupConfUpdateUsb:
      // Locate data blob from local USB device and apply changes. This function does not return
      //
      Status = ProcessDfciUsbInput ();
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a Failed to load configuration data from USB - %r\n", __FUNCTION__, Status));
        ASSERT (FALSE);
      }

      mSetupConfState = SetupConfExit;
      break;
    case SetupConfUpdateNetwork:
      // TODO: This should not return to previous settings,
      //       it should reboot if configuration changed
      Print (L"This function is still under construction, please stay tuned for more features!\n");
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
        Status = ProcessDfciSerialInput (KeyData.Key.UnicodeChar);
        if (EFI_ERROR (Status)) {
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
        Print (L"\nCurrent configurations are dumped Below in format of *.SVD:\n");
        Print (L"\n%a\n", StrBuf);
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
        DEBUG ((DEBUG_ERROR, "%a Error occurred while waiting for configuration selection - %r\n", __FUNCTION__, Status));
        ASSERT (FALSE);
      } else if ((KeyData.Key.UnicodeChar == CHAR_NULL) && (KeyData.Key.ScanCode == SCAN_ESC)) {
        mSetupConfState = SetupConfInit;
      }

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
