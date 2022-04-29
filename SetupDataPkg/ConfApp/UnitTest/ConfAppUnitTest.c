/** @file
  Unit tests of the ConfApp module

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
#include <Library/DxeServicesLib.h>

#include <Library/UnitTestLib.h>

#include "ConfApp.h"

#define UNIT_TEST_APP_NAME      "Conf Application Unit Tests"
#define UNIT_TEST_APP_VERSION   "1.0"

extern EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL  MockSimpleInput;

/**
  Entrypoint of configuration app. This function holds the main state machine for
  console based user interface.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some error occurs when executing this entry point.
**/
EFI_STATUS
EFIAPI
ConfAppEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  );

// Mocked version of DFCI_AUTHENTICATION_PROTOCOL instance.
EFI_STATUS
EFIAPI
MockGetEnrolledIdentities (
  IN CONST DFCI_AUTHENTICATION_PROTOCOL     *This,
  OUT      DFCI_IDENTITY_MASK               *EnrolledIdentities
  )
{
  assert_non_null (This);
  assert_non_null (EnrolledIdentities);

  *EnrolledIdentities = (DFCI_IDENTITY_MASK)mock();
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
MockAuthWithPW (
  IN  CONST DFCI_AUTHENTICATION_PROTOCOL  *This,
  IN  CONST CHAR16                        *Password OPTIONAL,
  IN  UINTN                                PasswordLength,
  OUT DFCI_AUTH_TOKEN                     *IdentityToken
  )
{
  assert_non_null (This);
  assert_null (Password);
  assert_int_equal (PasswordLength, 0);

  *IdentityToken = (DFCI_AUTH_TOKEN)mock();
  return EFI_SUCCESS;
}

DFCI_AUTHENTICATION_PROTOCOL MockAuthProtocol = {
  .GetEnrolledIdentities = MockGetEnrolledIdentities,
  .AuthWithPW = MockAuthWithPW,
};

extern enum ConfState_t_def mConfState;

VOID
SwitchMachineState (
  IN UINT32   State
)
{
  mConfState = State;
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
  SwitchMachineState ((UINT32)mock());
  return (EFI_STATUS)mock();
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
  SwitchMachineState ((UINT32)mock());
  return (EFI_STATUS)mock();
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
  SwitchMachineState ((UINT32)mock());
  return (EFI_STATUS)mock();
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
  CHAR8     Buffer[128];
  VA_LIST   Marker;
  UINTN     Ret;

  VA_START (Marker, Format);
  Ret = AsciiVSPrintUnicodeFormat (Buffer, sizeof (Buffer), Format, Marker);
  VA_END (Marker);

  DEBUG ((DEBUG_INFO, "%a", Buffer));

  return Ret;
}

EFI_STATUS
EFIAPI
MockLocateProtocol (
  IN  EFI_GUID *Protocol,
  IN  VOID *Registration, OPTIONAL
  OUT VOID      **Interface
  )
{
  DEBUG ((DEBUG_INFO, "%a - %g\n", __FUNCTION__, Protocol));
  // Check that this is the right protocol being located
  check_expected_ptr (Protocol);

  // Set the protcol to one of our mock protocols
  *Interface = (VOID *)mock ();

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
MockHandleProtocol (
  IN  EFI_HANDLE               Handle,
  IN  EFI_GUID                 *Protocol,
  OUT VOID                     **Interface
  )
{
  DEBUG ((DEBUG_INFO, "%a - %g\n", __FUNCTION__, Protocol));
  assert_non_null (Interface);
  assert_ptr_equal (Protocol, &gEfiSimpleTextInputExProtocolGuid);

  *Interface = &MockSimpleInput;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
MockSetWatchdogTimer (
  IN UINTN                    Timeout,
  IN UINT64                   WatchdogCode,
  IN UINTN                    DataSize,
  IN CHAR16                   *WatchdogData OPTIONAL
  )
{
  // Check that this is the right protocol being located
  assert_int_equal (Timeout, 0);
  assert_int_equal (WatchdogCode, 0);
  assert_int_equal (DataSize, 0);
  assert_null (WatchdogData);

  return (EFI_STATUS)mock();
}

EFI_STATUS
EFIAPI
MockWaitForEvent (
  IN  UINTN                    NumberOfEvents,
  IN  EFI_EVENT                *Event,
  OUT UINTN                    *Index
  )
{
  assert_int_equal (NumberOfEvents, 1);
  assert_int_equal (*Event, MockSimpleInput.WaitForKeyEx);
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
  .SetWatchdogTimer = MockSetWatchdogTimer,
  .HandleProtocol = MockHandleProtocol,
  .LocateProtocol = MockLocateProtocol,
  .WaitForEvent = MockWaitForEvent,
};

VOID
EFIAPI
ConfAppCleanup (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mConfState = 0;
}

/**
  Unit test for ConfAppEntry of ConfApp when selecting 1.

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
ConfAppEntrySelect1 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_KEY_DATA              KeyData1;
  EFI_KEY_DATA              KeyData2;
  BASE_LIBRARY_JUMP_BUFFER  JumpBuf;

  will_return (MockSetWatchdogTimer, EFI_SUCCESS);

  expect_value (MockEnableCursor, Visible, FALSE);
  will_return (MockEnableCursor, EFI_SUCCESS);

  expect_value (MockLocateProtocol, Protocol, &gDfciSettingAccessProtocolGuid);
  will_return (MockLocateProtocol, NULL);

  expect_value (MockLocateProtocol, Protocol, &gDfciAuthenticationProtocolGuid);
  will_return (MockLocateProtocol, &MockAuthProtocol);

  will_return (MockAuthWithPW, 0xFEEDF00D);
  will_return (MockGetEnrolledIdentities, DFCI_IDENTITY_LOCAL);

  expect_any_count (MockSetCursorPosition, Column, 1);
  expect_any_count (MockSetCursorPosition, Row, 1);
  will_return_count (MockSetCursorPosition, EFI_SUCCESS, 1);

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  KeyData1.Key.UnicodeChar = '1';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  will_return (SysInfoMgr, 6);
  will_return (SysInfoMgr, EFI_SUCCESS);

  KeyData2.Key.UnicodeChar = 'y';
  KeyData2.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData2);

  will_return (ResetCold, &JumpBuf);

  if (!SetJump (&JumpBuf)) {
    ConfAppEntry (NULL, NULL);
  }

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConfAppEntry of ConfApp when selecting 3.

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
ConfAppEntrySelect3 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_KEY_DATA              KeyData1;
  EFI_KEY_DATA              KeyData2;
  BASE_LIBRARY_JUMP_BUFFER  JumpBuf;

  will_return (MockSetWatchdogTimer, EFI_SUCCESS);

  expect_value (MockEnableCursor, Visible, FALSE);
  will_return (MockEnableCursor, EFI_SUCCESS);

  expect_value (MockLocateProtocol, Protocol, &gDfciSettingAccessProtocolGuid);
  will_return (MockLocateProtocol, NULL);

  expect_value (MockLocateProtocol, Protocol, &gDfciAuthenticationProtocolGuid);
  will_return (MockLocateProtocol, &MockAuthProtocol);

  will_return (MockAuthWithPW, 0xFEEDF00D);
  will_return (MockGetEnrolledIdentities, DFCI_IDENTITY_LOCAL);

  expect_any_count (MockSetCursorPosition, Column, 1);
  expect_any_count (MockSetCursorPosition, Row, 1);
  will_return_count (MockSetCursorPosition, EFI_SUCCESS, 1);

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  KeyData1.Key.UnicodeChar = '3';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  will_return (BootOptionMgr, 6);
  will_return (BootOptionMgr, EFI_SUCCESS);

  KeyData2.Key.UnicodeChar = 'y';
  KeyData2.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData2);

  will_return (ResetCold, &JumpBuf);

  if (!SetJump (&JumpBuf)) {
    ConfAppEntry (NULL, NULL);
  }

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConfAppEntry of ConfApp when selecting 4.

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
ConfAppEntrySelect4 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_KEY_DATA              KeyData1;
  EFI_KEY_DATA              KeyData2;
  BASE_LIBRARY_JUMP_BUFFER  JumpBuf;

  will_return (MockSetWatchdogTimer, EFI_SUCCESS);

  expect_value (MockEnableCursor, Visible, FALSE);
  will_return (MockEnableCursor, EFI_SUCCESS);

  expect_value (MockLocateProtocol, Protocol, &gDfciSettingAccessProtocolGuid);
  will_return (MockLocateProtocol, NULL);

  expect_value (MockLocateProtocol, Protocol, &gDfciAuthenticationProtocolGuid);
  will_return (MockLocateProtocol, &MockAuthProtocol);

  will_return (MockAuthWithPW, 0xFEEDF00D);
  will_return (MockGetEnrolledIdentities, DFCI_IDENTITY_LOCAL);

  expect_any_count (MockSetCursorPosition, Column, 1);
  expect_any_count (MockSetCursorPosition, Row, 1);
  will_return_count (MockSetCursorPosition, EFI_SUCCESS, 1);

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  KeyData1.Key.UnicodeChar = '4';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  will_return (SetupConfMgr, 6);
  will_return (SetupConfMgr, EFI_SUCCESS);

  KeyData2.Key.UnicodeChar = 'y';
  KeyData2.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData2);

  will_return (ResetCold, &JumpBuf);

  if (!SetJump (&JumpBuf)) {
    ConfAppEntry (NULL, NULL);
  }

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConfAppEntry of ConfApp when selecting h.

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
ConfAppEntrySelectH (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_KEY_DATA              KeyData1;
  EFI_KEY_DATA              KeyData2;
  EFI_KEY_DATA              KeyData3;
  BASE_LIBRARY_JUMP_BUFFER  JumpBuf;

  will_return (MockSetWatchdogTimer, EFI_SUCCESS);

  expect_value (MockEnableCursor, Visible, FALSE);
  will_return (MockEnableCursor, EFI_SUCCESS);

  expect_value (MockLocateProtocol, Protocol, &gDfciSettingAccessProtocolGuid);
  will_return (MockLocateProtocol, NULL);

  expect_value (MockLocateProtocol, Protocol, &gDfciAuthenticationProtocolGuid);
  will_return (MockLocateProtocol, &MockAuthProtocol);

  will_return (MockAuthWithPW, 0xFEEDF00D);
  will_return (MockGetEnrolledIdentities, DFCI_IDENTITY_LOCAL);

  expect_any_count (MockSetCursorPosition, Column, 2);
  expect_any_count (MockSetCursorPosition, Row, 2);
  will_return_count (MockSetCursorPosition, EFI_SUCCESS, 2);

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  KeyData1.Key.UnicodeChar = 'h';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  will_return (MockClearScreen, EFI_SUCCESS);

  KeyData2.Key.UnicodeChar = CHAR_NULL;
  KeyData2.Key.ScanCode    = SCAN_ESC;
  will_return (MockReadKey, &KeyData2);

  KeyData3.Key.UnicodeChar = 'y';
  KeyData3.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData3);

  will_return (ResetCold, &JumpBuf);

  if (!SetJump (&JumpBuf)) {
    ConfAppEntry (NULL, NULL);
  }

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConfAppEntry of ConfApp when selecting ESC.

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
ConfAppEntrySelectEsc (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_KEY_DATA              KeyData1;
  EFI_KEY_DATA              KeyData2;
  BASE_LIBRARY_JUMP_BUFFER  JumpBuf;

  will_return (MockSetWatchdogTimer, EFI_SUCCESS);

  expect_value (MockEnableCursor, Visible, FALSE);
  will_return (MockEnableCursor, EFI_SUCCESS);

  expect_value (MockLocateProtocol, Protocol, &gDfciSettingAccessProtocolGuid);
  will_return (MockLocateProtocol, NULL);

  expect_value (MockLocateProtocol, Protocol, &gDfciAuthenticationProtocolGuid);
  will_return (MockLocateProtocol, &MockAuthProtocol);

  will_return (MockAuthWithPW, 0xFEEDF00D);
  will_return (MockGetEnrolledIdentities, DFCI_IDENTITY_LOCAL);

  expect_any_count (MockSetCursorPosition, Column, 1);
  expect_any_count (MockSetCursorPosition, Row, 1);
  will_return_count (MockSetCursorPosition, EFI_SUCCESS, 1);

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  KeyData1.Key.UnicodeChar = CHAR_NULL;
  KeyData1.Key.ScanCode    = SCAN_ESC;
  will_return (MockReadKey, &KeyData1);

  KeyData2.Key.UnicodeChar = 'y';
  KeyData2.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData2);

  will_return (ResetCold, &JumpBuf);

  if (!SetJump (&JumpBuf)) {
    ConfAppEntry (NULL, NULL);
  }

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConfAppEntry of ConfApp when selecting others.

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
ConfAppEntrySelectOther (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_KEY_DATA              KeyData1;
  EFI_KEY_DATA              KeyData2;
  EFI_KEY_DATA              KeyData3;
  BASE_LIBRARY_JUMP_BUFFER  JumpBuf;

  will_return (MockSetWatchdogTimer, EFI_SUCCESS);

  expect_value (MockEnableCursor, Visible, FALSE);
  will_return (MockEnableCursor, EFI_SUCCESS);

  expect_value (MockLocateProtocol, Protocol, &gDfciSettingAccessProtocolGuid);
  will_return (MockLocateProtocol, NULL);

  expect_value (MockLocateProtocol, Protocol, &gDfciAuthenticationProtocolGuid);
  will_return (MockLocateProtocol, &MockAuthProtocol);

  will_return (MockAuthWithPW, 0xFEEDF00D);
  will_return (MockGetEnrolledIdentities, DFCI_IDENTITY_LOCAL);

  expect_any_count (MockSetCursorPosition, Column, 1);
  expect_any_count (MockSetCursorPosition, Row, 1);
  will_return_count (MockSetCursorPosition, EFI_SUCCESS, 1);

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  KeyData1.Key.UnicodeChar = 'q';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  KeyData2.Key.UnicodeChar = CHAR_NULL;
  KeyData2.Key.ScanCode    = SCAN_ESC;
  will_return (MockReadKey, &KeyData2);

  KeyData3.Key.UnicodeChar = 'y';
  KeyData3.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData3);

  will_return (ResetCold, &JumpBuf);

  if (!SetJump (&JumpBuf)) {
    ConfAppEntry (NULL, NULL);
  }

  return UNIT_TEST_PASSED;
}

/**
  Initialze the unit test framework, suite, and unit tests for the
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
  // Populate the ConfApp Unit Test Suite.
  //
  Status = CreateUnitTestSuite (&MiscTests, Framework, "ConfApp Misc Tests", "ConfApp.Misc", NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in CreateUnitTestSuite for MiscTests\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  //
  // --------------Suite-----------Description--------------Name----------Function--------Pre---Post-------------------Context-----------
  //
  AddTestCase (MiscTests, "ConfApp Select 1 should go to System Info", "Select1", ConfAppEntrySelect1, NULL, ConfAppCleanup, NULL);
  // TODO: this is not supported yet
  // AddTestCase (MiscTests, "ConfApp Select 2 should go to Secure Boot", "Select2", ConfAppEntrySelect2, NULL, ConfAppCleanup, NULL);
  AddTestCase (MiscTests, "ConfApp Select 3 should go to Boot Options", "Select3", ConfAppEntrySelect3, NULL, ConfAppCleanup, NULL);
  AddTestCase (MiscTests, "ConfApp Select 4 should go to Setup Option", "Select4", ConfAppEntrySelect4, NULL, ConfAppCleanup, NULL);
  AddTestCase (MiscTests, "ConfApp Select h should reprint the options", "SelectH", ConfAppEntrySelectH, NULL, ConfAppCleanup, NULL);
  AddTestCase (MiscTests, "ConfApp Select ESC should confirm reboot", "SelectEsc", ConfAppEntrySelectEsc, NULL, ConfAppCleanup, NULL);
  AddTestCase (MiscTests, "ConfApp Select other should do nothing", "SelectOther", ConfAppEntrySelectOther, NULL, ConfAppCleanup, NULL);

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
