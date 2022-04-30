/** @file
  Unit tests of the Setup Configuration page of ConfApp module

  Copyright (C) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <Uefi.h>
#include <Pi/PiFirmwareFile.h>
#include <DfciSystemSettingTypes.h>
#include <Protocol/VariablePolicy.h>
#include <Protocol/DfciSettingsProvider.h>

#include <Library/BaseLib.h>
#include <Library/PrintLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/ConfigDataLib.h>
#include <Library/UefiBootManagerLib.h>

#include <Library/UnitTestLib.h>

#include <Good_Config_Data.h>
#include "ConfApp.h"

#define UNIT_TEST_APP_NAME     "Conf Application Setup Configuration Unit Tests"
#define UNIT_TEST_APP_VERSION  "1.0"

#define KNOWN_GOOD_TAG_0xF0   0xF0
#define KNOWN_GOOD_TAG_0x70   0x70
#define KNOWN_GOOD_TAG_0x280  0x280
#define KNOWN_GOOD_TAG_0x180  0x180
#define KNOWN_GOOD_TAG_0x200  0x200
#define KNOWN_GOOD_TAG_0x10   0x10
#define KNOWN_GOOD_TAG_0x80   0x80

#define KNOWN_GOOD_TAG_COUNT     7
#define SINGLE_CONF_DATA_ID_LEN  (sizeof (SINGLE_SETTING_PROVIDER_START) + sizeof (UINT32) * 2)

extern EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL  MockSimpleInput;
extern enum SetupConfState_t_def          mSetupConfState;

typedef struct {
  UINTN    Tag;
  UINT8    *Data;
  UINTN    DataSize;
} TAG_DATA;

TAG_DATA  mKnownGoodTags[KNOWN_GOOD_TAG_COUNT] = {
  { KNOWN_GOOD_TAG_0xF0,  mGood_Tag_0xF0,  sizeof (mGood_Tag_0xF0)  },
  { KNOWN_GOOD_TAG_0x70,  mGood_Tag_0x70,  sizeof (mGood_Tag_0x70)  },
  { KNOWN_GOOD_TAG_0x280, mGood_Tag_0x280, sizeof (mGood_Tag_0x280) },
  { KNOWN_GOOD_TAG_0x180, mGood_Tag_0x180, sizeof (mGood_Tag_0x180) },
  { KNOWN_GOOD_TAG_0x200, mGood_Tag_0x200, sizeof (mGood_Tag_0x200) },
  { KNOWN_GOOD_TAG_0x10,  mGood_Tag_0x10,  sizeof (mGood_Tag_0x10)  },
  { KNOWN_GOOD_TAG_0x80,  mGood_Tag_0x80,  sizeof (mGood_Tag_0x80)  }
};

/*
Set a single setting

@param[in] This:       Access Protocol
@param[in] Id:         Setting ID to set
@param[in] AuthToken:  A valid auth token to apply the setting using.  This auth token will be validated
                       to check permissions for changing the setting.
@param[in] Type:       Type that caller expects this setting to be.
@param[in] Value:      A pointer to a datatype defined by the Type for this setting.
@param[in,out] Flags:  Informational Flags passed to the SET and/or Returned as a result of the set

@retval EFI_SUCCESS if setting could be set.  Check flags for other info (reset required, etc)
@retval Error - Setting not set.

*/
EFI_STATUS
EFIAPI
MockSet (
  IN  CONST DFCI_SETTING_ACCESS_PROTOCOL  *This,
  IN  DFCI_SETTING_ID_STRING              Id,
  IN  CONST DFCI_AUTH_TOKEN               *AuthToken,
  IN  DFCI_SETTING_TYPE                   Type,
  IN  UINTN                               ValueSize,
  IN  CONST VOID                          *Value,
  IN OUT DFCI_SETTING_FLAGS               *Flags
  )
{
  assert_non_null (This);
  assert_ptr_equal (AuthToken, &mAuthToken);
  assert_int_equal (Type, DFCI_SETTING_TYPE_BINARY);
  assert_non_null (ValueSize);
  assert_non_null (Flags);
  check_expected (Id);
  check_expected (ValueSize);
  check_expected (Value);

  *Flags |= DFCI_SETTING_FLAGS_OUT_REBOOT_REQUIRED;
  return EFI_SUCCESS;
}

/*
Get a single setting

@param[in] This:         Access Protocol
@param[in] Id:           Setting ID to Get
@param[in] AuthToken:    An optional auth token* to use to check permission of setting.  This auth token will be validated
                         to check permissions for changing the setting which will be reported in flags if valid.
@param[in] Type:         Type that caller expects this setting to be.
@param[in,out] ValueSize IN=Size of location to store value
                         OUT=Size of value stored
@param[out] Value:       A pointer to a datatype defined by the Type for this setting.
@param[IN OUT] Flags     Optional Informational flags passed back from the Get operation.  If the Auth Token is valid write access will be set in
                         flags for the given auth.

@retval EFI_SUCCESS if setting could be set.  Check flags for other info (reset required, etc)
@retval Error - couldn't get setting.

*/
EFI_STATUS
EFIAPI
MockGet (
  IN  CONST DFCI_SETTING_ACCESS_PROTOCOL  *This,
  IN  DFCI_SETTING_ID_STRING              Id,
  IN  CONST DFCI_AUTH_TOKEN               *AuthToken  OPTIONAL,
  IN  DFCI_SETTING_TYPE                   Type,
  IN  OUT UINTN                           *ValueSize,
  OUT     VOID                            *Value,
  IN  OUT DFCI_SETTING_FLAGS              *Flags OPTIONAL
  )
{
  UINTN  Size;
  VOID   *Data;

  assert_non_null (This);
  assert_ptr_equal (AuthToken, &mAuthToken);
  assert_int_equal (Type, DFCI_SETTING_TYPE_BINARY);
  assert_non_null (ValueSize);
  DEBUG ((DEBUG_INFO, "%a Settings get ID: %a\n", __FUNCTION__, Id));
  check_expected (Id);

  Size = (UINTN)mock ();
  if (*ValueSize < Size) {
    *ValueSize = Size;
    return EFI_BUFFER_TOO_SMALL;
  }

  *ValueSize = Size;
  Data       = (VOID *)mock ();
  CopyMem (Value, Data, Size);
  return EFI_SUCCESS;
}

DFCI_SETTING_ACCESS_PROTOCOL  MockSettingAccess = {
  .Set = MockSet,
  .Get = MockGet,
};

/**
 * BuildUsbRequest
 *
 * @param[in]   FileNameExtension - Extension for file name
 * @param[out]  filename          - Name of the file on USB to retrieve
 *
 **/
EFI_STATUS
EFIAPI
BuildUsbRequest (
  IN  CHAR16  *FileExtension,
  OUT CHAR16  **FileName
  )
{
  CHAR16  *ret_buf;

  assert_memory_equal (FileExtension, L".svd", sizeof (L".svd"));
  assert_non_null (FileName);

  ret_buf = (CHAR16 *)mock ();

  *FileName = AllocateCopyPool (StrSize (ret_buf), ret_buf);
  return EFI_SUCCESS;
}

/**
*
*  Request a Json Dfci settings packet.
*
*  @param[in]     FileName        What file to read.
*  @param[out]    JsonString      Where to store the Json String
*  @param[out]    JsonStringSize  Size of Json String
*
*  @retval   Status               Always returns success.
*
**/
EFI_STATUS
EFIAPI
DfciRequestJsonFromUSB (
  IN  CHAR16  *FileName,
  OUT CHAR8   **JsonString,
  OUT UINTN   *JsonStringSize
  )
{
  CHAR8  *Ret_Buff;

  check_expected (FileName);
  assert_non_null (JsonStringSize);

  *JsonStringSize = (UINTN)mock ();
  Ret_Buff        = (UINT8 *)mock ();
  *JsonString     = AllocateCopyPool (*JsonStringSize, Ret_Buff);

  return EFI_SUCCESS;
}

/**
  State machine for system information page. It will display fundamental information, including
  UEFI version, system time, DFCI identity and configuration settings.

  @retval EFI_SUCCESS           This iteration of state machine proceeds successfully.
  @retval Others                Failed to wait for valid keystrokes or application of
                                new configuration data failed.
**/
EFI_STATUS
EFIAPI
SysInfoMgr (
  VOID
  )
{
  // Not used
  ASSERT (FALSE);
  return EFI_ACCESS_DENIED;
}

/**
  State machine for boot option page. It will react to user input from keystroke
  to boot to selected boot option or go back to previous page.

  @retval EFI_SUCCESS           This iteration of state machine proceeds successfully.
  @retval Others                Failed to wait for valid keystrokes. Failed boot will not
                                return error code but cause reboot directly.
**/
EFI_STATUS
EFIAPI
BootOptionMgr (
  VOID
  )
{
  // Not used
  ASSERT (FALSE);
  return EFI_ACCESS_DENIED;
}

/**
  This function will connect all the system driver to controller
  first, and then special connect the default console, this make
  sure all the system controller available and the platform default
  console connected.

**/
VOID
EFIAPI
EfiBootManagerConnectAll (
  VOID
  )
{
  return;
}

/**
  Prints a formatted Unicode string to the console output device specified by
  ConOut defined in the EFI_SYSTEM_TABLE.

  This function prints a formatted Unicode string to the console output device
  specified by ConOut in EFI_SYSTEM_TABLE and returns the number of Unicode
  characters that printed to ConOut.  If the length of the formatted Unicode
  string is greater than PcdUefiLibMaxPrintBufferSize, then only the first
  PcdUefiLibMaxPrintBufferSize characters are sent to ConOut.
  If Format is NULL, then ASSERT().
  If Format is not aligned on a 16-bit boundary, then ASSERT().
  If gST->ConOut is NULL, then ASSERT().

  @param Format   A Null-terminated Unicode format string.
  @param ...      A Variable argument list whose contents are accessed based
                  on the format string specified by Format.

  @return The number of Unicode characters printed to ConOut.

**/
UINTN
EFIAPI
Print (
  IN CONST CHAR16  *Format,
  ...
  )
{
  CHAR8    Buffer[128];
  VA_LIST  Marker;
  UINTN    Ret;

  VA_START (Marker, Format);
  Ret = AsciiVSPrintUnicodeFormat (Buffer, sizeof (Buffer), Format, Marker);
  VA_END (Marker);

  DEBUG ((DEBUG_INFO, "%a", Buffer));

  return Ret;
}

EFI_STATUS
EFIAPI
MockGetTime (
  OUT  EFI_TIME               *Time,
  OUT  EFI_TIME_CAPABILITIES  *Capabilities OPTIONAL
  )
{
  EFI_TIME  DefaultPayloadTimestamp = {
    22,   // Year (2022)
    4,    // Month (April)
    29,   // Day (29)
  };

  assert_non_null (Time);
  CopyMem (Time, &DefaultPayloadTimestamp, sizeof (EFI_TIME));

  return EFI_SUCCESS;
}

///
/// Mock version of the UEFI Runtime Services Table
///
EFI_RUNTIME_SERVICES  MockRuntime = {
  .GetTime = MockGetTime
};

EFI_STATUS
EFIAPI
MockWaitForEvent (
  IN  UINTN      NumberOfEvents,
  IN  EFI_EVENT  *Event,
  OUT UINTN      *Index
  )
{
  assert_int_equal (NumberOfEvents, 1);
  assert_int_equal (Event[0], MockSimpleInput.WaitForKeyEx);
  assert_non_null (Index);

  *Index = 0;
  return EFI_SUCCESS;
}

///
/// Mock version of the UEFI Boot Services Table
///
EFI_BOOT_SERVICES  MockBoot = {
  {
    EFI_BOOT_SERVICES_SIGNATURE,
    EFI_BOOT_SERVICES_REVISION,
    sizeof (EFI_BOOT_SERVICES),
    0,
    0
  },
  .WaitForEvent = MockWaitForEvent,
};

VOID
EFIAPI
SetupConfCleanup (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mSetupConfState = 0;
}

/**
  Unit test for SetupConf page when selecting ESC.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
ConfAppSetupConfInit (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  expect_any (MockSetCursorPosition, Column);
  expect_any (MockSetCursorPosition, Row);
  will_return (MockSetCursorPosition, EFI_SUCCESS);

  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SetupConf page when selecting ESC.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
ConfAppSetupConfSelectEsc (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS    Status;
  EFI_KEY_DATA  KeyData1;

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  expect_any (MockSetCursorPosition, Column);
  expect_any (MockSetCursorPosition, Row);
  will_return (MockSetCursorPosition, EFI_SUCCESS);

  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSetupConfState, 1);

  mSimpleTextInEx = &MockSimpleInput;

  KeyData1.Key.UnicodeChar = CHAR_NULL;
  KeyData1.Key.ScanCode    = SCAN_ESC;
  will_return (MockReadKey, &KeyData1);

  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSetupConfState, 8);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SetupConf page when selecting others.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
ConfAppSetupConfSelectOther (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS    Status;
  EFI_KEY_DATA  KeyData1;

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  expect_any (MockSetCursorPosition, Column);
  expect_any (MockSetCursorPosition, Row);
  will_return (MockSetCursorPosition, EFI_SUCCESS);

  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSetupConfState, 1);

  mSimpleTextInEx = &MockSimpleInput;

  KeyData1.Key.UnicodeChar = 'X';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSetupConfState, 1);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SetupConf page when selecting configure from USB.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
ConfAppSetupConfSelectUsb (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                Status;
  EFI_KEY_DATA              KeyData1;
  BASE_LIBRARY_JUMP_BUFFER  JumpBuf;

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  // Expect the prints twice
  expect_any (MockSetCursorPosition, Column);
  expect_any (MockSetCursorPosition, Row);
  will_return (MockSetCursorPosition, EFI_SUCCESS);

  // Initial run
  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSetupConfState, 1);

  mSimpleTextInEx = &MockSimpleInput;

  KeyData1.Key.UnicodeChar = '1';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSetupConfState, 2);

  mSettingAccess = &MockSettingAccess;

  will_return (BuildUsbRequest, L"Test");

  expect_memory (DfciRequestJsonFromUSB, FileName, L"Test", sizeof (L"Test"));
  will_return (DfciRequestJsonFromUSB, sizeof (KNOWN_GOOD_XML) - 1);
  will_return (DfciRequestJsonFromUSB, KNOWN_GOOD_XML);

  expect_string (MockSet, Id, DFCI_OEM_SETTING_ID__CONF);
  expect_value (MockSet, ValueSize, sizeof (mKnown_Good_Config_Data));
  expect_memory (MockSet, Value, mKnown_Good_Config_Data, sizeof (mKnown_Good_Config_Data));

  will_return (ResetCold, &JumpBuf);

  if (!SetJump (&JumpBuf)) {
    SetupConfMgr ();
  }

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SetupConf page when selecting configure from serial.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
ConfAppSetupConfSelectSerial (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                Status;
  EFI_KEY_DATA              KeyData1;
  BASE_LIBRARY_JUMP_BUFFER  JumpBuf;
  UINTN                     Index;
  CHAR8                     *KnowGoodXml;

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  // Expect the prints twice
  expect_any (MockSetCursorPosition, Column);
  expect_any (MockSetCursorPosition, Row);
  will_return (MockSetCursorPosition, EFI_SUCCESS);

  // Initial run
  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSetupConfState, 1);

  mSimpleTextInEx = &MockSimpleInput;

  KeyData1.Key.UnicodeChar = '3';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSetupConfState, 4);

  mSettingAccess = &MockSettingAccess;

  Index       = 0;
  KnowGoodXml = KNOWN_GOOD_XML;
  while (KnowGoodXml[Index] != 0) {
    KeyData1.Key.UnicodeChar = KnowGoodXml[Index];
    KeyData1.Key.ScanCode    = SCAN_NULL;
    will_return (MockReadKey, &KeyData1);
    Status = SetupConfMgr ();
    Index++;
  }

  KeyData1.Key.UnicodeChar = CHAR_CARRIAGE_RETURN;
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  expect_string (MockSet, Id, DFCI_OEM_SETTING_ID__CONF);
  expect_value (MockSet, ValueSize, sizeof (mKnown_Good_Config_Data));
  expect_memory (MockSet, Value, mKnown_Good_Config_Data, sizeof (mKnown_Good_Config_Data));

  will_return (ResetCold, &JumpBuf);

  if (!SetJump (&JumpBuf)) {
    SetupConfMgr ();
  }

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SetupConf page when selecting configure from serial and return in the middle.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
ConfAppSetupConfSelectSerialEsc (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS    Status;
  EFI_KEY_DATA  KeyData1;
  UINTN         Index;
  CHAR8         *KnowGoodXml;

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  // Expect the prints twice
  expect_any (MockSetCursorPosition, Column);
  expect_any (MockSetCursorPosition, Row);
  will_return (MockSetCursorPosition, EFI_SUCCESS);

  // Initial run
  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSetupConfState, 1);

  mSimpleTextInEx = &MockSimpleInput;

  KeyData1.Key.UnicodeChar = '3';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSetupConfState, 4);

  mSettingAccess = &MockSettingAccess;

  Index       = 0;
  KnowGoodXml = KNOWN_GOOD_XML;
  while (KnowGoodXml[Index] != 0) {
    KeyData1.Key.UnicodeChar = KnowGoodXml[Index];
    KeyData1.Key.ScanCode    = SCAN_NULL;
    will_return (MockReadKey, &KeyData1);
    Status = SetupConfMgr ();
    Index++;
  }

  KeyData1.Key.UnicodeChar = CHAR_NULL;
  KeyData1.Key.ScanCode    = SCAN_ESC;
  will_return (MockReadKey, &KeyData1);

  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSetupConfState, 8);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SetupConf page when selecting dumping configure from serial.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
ConfAppSetupConfDumpSerial (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS    Status;
  EFI_KEY_DATA  KeyData1;
  CHAR8         *ComparePtr[KNOWN_GOOD_TAG_COUNT];
  UINTN         Index;

  will_return_count (MockClearScreen, EFI_SUCCESS, 2);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  // Expect the prints twice
  expect_any_count (MockSetCursorPosition, Column, 2);
  expect_any_count (MockSetCursorPosition, Row, 2);
  will_return_count (MockSetCursorPosition, EFI_SUCCESS, 2);

  // Initial run
  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSetupConfState, 1);

  mSimpleTextInEx = &MockSimpleInput;

  KeyData1.Key.UnicodeChar = '4';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSetupConfState, 6);

  mSettingAccess = &MockSettingAccess;

  expect_string (MockGet, Id, DFCI_OEM_SETTING_ID__CONF);
  will_return (MockGet, sizeof (mKnown_Good_Config_Data));
  expect_string (MockGet, Id, DFCI_OEM_SETTING_ID__CONF);
  will_return (MockGet, sizeof (mKnown_Good_Config_Data));
  will_return (MockGet, mKnown_Good_Config_Data);

  for (Index = 0; Index < KNOWN_GOOD_TAG_COUNT; Index++) {
    ComparePtr[Index] = AllocatePool (SINGLE_CONF_DATA_ID_LEN);
    AsciiSPrint (ComparePtr[Index], SINGLE_CONF_DATA_ID_LEN, SINGLE_SETTING_PROVIDER_TEMPLATE, mKnownGoodTags[Index].Tag);
    expect_string (MockGet, Id, ComparePtr[Index]);
    will_return (MockGet, mKnownGoodTags[Index].DataSize);
    expect_string (MockGet, Id, ComparePtr[Index]);
    will_return (MockGet, mKnownGoodTags[Index].DataSize);
    will_return (MockGet, mKnownGoodTags[Index].Data);
  }

  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSetupConfState, 7);

  Index = 0;
  while (Index < KNOWN_GOOD_TAG_COUNT) {
    FreePool (ComparePtr[Index]);
    Index++;
  }

  return UNIT_TEST_PASSED;
}

/**
  Initialize the unit test framework, suite, and unit tests for the
  ConfApp and run the ConfApp unit test.

  @retval  EFI_SUCCESS           All test cases were dispatched.
  @retval  EFI_OUT_OF_RESOURCES  There are not enough resources available to
                                 initialize the unit tests.
**/
STATIC
EFI_STATUS
EFIAPI
UnitTestingEntry (
  VOID
  )
{
  EFI_STATUS                  Status;
  UNIT_TEST_FRAMEWORK_HANDLE  Framework;
  UNIT_TEST_SUITE_HANDLE      MiscTests;

  Framework = NULL;

  DEBUG ((DEBUG_INFO, "%a v%a\n", UNIT_TEST_APP_NAME, UNIT_TEST_APP_VERSION));

  //
  // Start setting up the test framework for running the tests.
  //
  Status = InitUnitTestFramework (&Framework, UNIT_TEST_APP_NAME, gEfiCallerBaseName, UNIT_TEST_APP_VERSION);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in InitUnitTestFramework. Status = %r\n", Status));
    goto EXIT;
  }

  //
  // Populate the ConfDataSettingProvider Unit Test Suite.
  //
  Status = CreateUnitTestSuite (&MiscTests, Framework, "ConfDataSettingProvider Misc Tests", "ConfDataSettingProvider.Misc", NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in CreateUnitTestSuite for MiscTests\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  //
  // --------------Suite-----------Description--------------Name----------Function--------Pre---Post-------------------Context-----------
  //
  AddTestCase (MiscTests, "Setup Configuration page should initialize properly", "NormalInit", ConfAppSetupConfInit, NULL, SetupConfCleanup, NULL);
  AddTestCase (MiscTests, "Setup Configuration page select Esc should go to previous menu", "SelectEsc", ConfAppSetupConfSelectEsc, NULL, SetupConfCleanup, NULL);
  AddTestCase (MiscTests, "Setup Configuration page select others should do nothing", "SelectOther", ConfAppSetupConfSelectOther, NULL, SetupConfCleanup, NULL);
  AddTestCase (MiscTests, "Setup Configuration page should setup configuration from USB", "SelectUsb", ConfAppSetupConfSelectUsb, NULL, SetupConfCleanup, NULL);
  AddTestCase (MiscTests, "Setup Configuration page should setup configuration from serial", "SelectSerial", ConfAppSetupConfSelectSerial, NULL, SetupConfCleanup, NULL);
  AddTestCase (MiscTests, "Setup Configuration page should return with ESC key during serial transport", "SelectSerial", ConfAppSetupConfSelectSerialEsc, NULL, SetupConfCleanup, NULL);
  AddTestCase (MiscTests, "Setup Configuration page should dump all configurations from serial", "ConfDump", ConfAppSetupConfDumpSerial, NULL, SetupConfCleanup, NULL);

  //
  // Execute the tests.
  //
  Status = RunAllTestSuites (Framework);

EXIT:
  if (Framework) {
    FreeUnitTestFramework (Framework);
  }

  return Status;
}

/**
  Standard POSIX C entry point for host based unit test execution.
**/
int
main (
  int   argc,
  char  *argv[]
  )
{
  return UnitTestingEntry ();
}
