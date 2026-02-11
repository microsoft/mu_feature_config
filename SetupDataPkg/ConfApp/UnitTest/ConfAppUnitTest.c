/** @file
  Unit tests of the main page of ConfApp module

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
#include <Protocol/VariablePolicy.h>

#include <Library/BaseLib.h>
#include <Library/PrintLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DxeServicesLib.h>

#include <Library/UnitTestLib.h>

#include "ConfApp.h"

#define UNIT_TEST_APP_NAME     "Conf Application Unit Tests"
#define UNIT_TEST_APP_VERSION  "1.0"

extern EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL  MockSimpleInput;
extern ConfState_t                        mConfState;
extern BOOLEAN                            MainStateMachineRunning;
extern volatile BOOLEAN                   gResetCalled;

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

/**
  Helper function to switch main state machine.

  @param[in] State    The target state the main state machine transition to.

**/
VOID
SwitchMachineState (
  IN ConfState_t  State
  )
{
  mConfState = State;
}

/**
  State machine for system information page. It will display fundamental information, including
  UEFI version, system time, and configuration settings.

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
  SwitchMachineState ((ConfState_t)mock ());
  return (EFI_STATUS)mock ();
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
  SwitchMachineState ((ConfState_t)mock ());
  return (EFI_STATUS)mock ();
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
  SwitchMachineState ((ConfState_t)mock ());
  return (EFI_STATUS)mock ();
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
  Mock version of Print.

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

/**
 * Mock implementation of CpuDeadLoop to prevent actual deadlocks during testing.
 * This function immediately returns instead of causing an infinite loop,
 * allowing tests to run without hanging the system.
 *
 * @return None
 */
VOID
EFIAPI
MockCpuDeadLoop (
  VOID
  )
{
  return;
}

/**
  Mocked version of LocateProtocol.

  @param[in]  Protocol          Provides the protocol to search for.
  @param[in]  Registration      Optional registration key returned from
                                RegisterProtocolNotify().
  @param[out]  Interface        On return, a pointer to the first interface that matches Protocol and
                                Registration.

  @retval EFI_SUCCESS           A protocol instance matching Protocol was found and returned in
                                Interface.
  @retval EFI_NOT_FOUND         No protocol instances were found that match Protocol and
                                Registration.
  @retval EFI_INVALID_PARAMETER Interface is NULL.
                                Protocol is NULL.

**/
EFI_STATUS
EFIAPI
MockLocateProtocol (
  IN  EFI_GUID *Protocol,
  IN  VOID *Registration, OPTIONAL
  OUT VOID      **Interface
  )
{
  DEBUG ((DEBUG_INFO, "%a - %g\n", __func__, Protocol));
  // Check that this is the right protocol being located
  check_expected_ptr (Protocol);

  // Set the protocol to one of our mock protocols
  *Interface = (VOID *)mock ();

  return EFI_SUCCESS;
}

/**
  Mocked version of HandleProtocol.

  @param[in]   Handle           The handle being queried.
  @param[in]   Protocol         The published unique identifier of the protocol.
  @param[out]  Interface        Supplies the address where a pointer to the corresponding Protocol
                                Interface is returned.

  @retval EFI_SUCCESS           The interface information for the specified protocol was returned.
  @retval EFI_UNSUPPORTED       The device does not support the specified protocol.
  @retval EFI_INVALID_PARAMETER Handle is NULL.
  @retval EFI_INVALID_PARAMETER Protocol is NULL.
  @retval EFI_INVALID_PARAMETER Interface is NULL.

**/
EFI_STATUS
EFIAPI
MockHandleProtocol (
  IN  EFI_HANDLE  Handle,
  IN  EFI_GUID    *Protocol,
  OUT VOID        **Interface
  )
{
  DEBUG ((DEBUG_INFO, "%a - %g\n", __func__, Protocol));
  assert_non_null (Interface);
  assert_ptr_equal (Protocol, &gEfiSimpleTextInputExProtocolGuid);

  *Interface = &MockSimpleInput;

  return EFI_SUCCESS;
}

/**
  Mocked version of SetWatchdogTimer.

  @param[in]  Timeout           The number of seconds to set the watchdog timer to.
  @param[in]  WatchdogCode      The numeric code to log on a watchdog timer timeout event.
  @param[in]  DataSize          The size, in bytes, of WatchdogData.
  @param[in]  WatchdogData      A data buffer that includes a Null-terminated string, optionally
                                followed by additional binary data.

  @retval EFI_SUCCESS           The timeout has been set.
  @retval EFI_INVALID_PARAMETER The supplied WatchdogCode is invalid.
  @retval EFI_UNSUPPORTED       The system does not have a watchdog timer.
  @retval EFI_DEVICE_ERROR      The watchdog timer could not be programmed due to a hardware
                                error.

**/
EFI_STATUS
EFIAPI
MockSetWatchdogTimer (
  IN UINTN   Timeout,
  IN UINT64  WatchdogCode,
  IN UINTN   DataSize,
  IN CHAR16  *WatchdogData OPTIONAL
  )
{
  // Check that this is the right protocol being located
  assert_int_equal (Timeout, 0);
  assert_int_equal (WatchdogCode, 0);
  assert_int_equal (DataSize, 0);
  assert_null (WatchdogData);

  return (EFI_STATUS)mock ();
}

/**
  Mocked version of WaitForEvent.

  @param[in]   NumberOfEvents   The number of events in the Event array.
  @param[in]   Event            An array of EFI_EVENT.
  @param[out]  Index            The pointer to the index of the event which satisfied the wait condition.

  @retval EFI_SUCCESS           The event indicated by Index was signaled.
  @retval EFI_INVALID_PARAMETER 1) NumberOfEvents is 0.
                                2) The event indicated by Index is of type
                                   EVT_NOTIFY_SIGNAL.
  @retval EFI_UNSUPPORTED       The current TPL is not TPL_APPLICATION.

**/
EFI_STATUS
EFIAPI
MockWaitForEvent (
  IN  UINTN      NumberOfEvents,
  IN  EFI_EVENT  *Event,
  OUT UINTN      *Index
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
  .HandleProtocol   = MockHandleProtocol,
  .LocateProtocol   = MockLocateProtocol,
  .WaitForEvent     = MockWaitForEvent,
};

VOID
EFIAPI
ConfAppCleanup (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mConfState              = 0;
  MainStateMachineRunning = TRUE;
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
  EFI_KEY_DATA  KeyData1;
  EFI_KEY_DATA  KeyData2;

  DEBUG ((DEBUG_INFO, "ConfAppEntrySelect1 called \n"));
  will_return (MockSetWatchdogTimer, EFI_SUCCESS);

  expect_value (MockEnableCursor, Visible, FALSE);
  will_return (MockEnableCursor, EFI_SUCCESS);

  expect_any_count (MockSetCursorPosition, Column, 1);
  expect_any_count (MockSetCursorPosition, Row, 1);
  will_return_count (MockSetCursorPosition, EFI_SUCCESS, 1);

  will_return_always (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  KeyData1.Key.UnicodeChar = '1';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  will_return (SysInfoMgr, MainExit);
  will_return (SysInfoMgr, EFI_SUCCESS);

  KeyData2.Key.UnicodeChar = 'y';
  KeyData2.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData2);

  gResetCalled = FALSE;

  expect_value (ResetSystemWithSubtype, ResetType, EfiResetCold);
  expect_value (ResetSystemWithSubtype, ResetSubtype, &gConfAppResetGuid);

  ConfAppEntry (NULL, NULL);
  UT_ASSERT_TRUE (gResetCalled); // Assert that reset was called

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConfAppEntry of ConfApp when selecting 2.

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
ConfAppEntrySelect2 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_KEY_DATA  KeyData1;
  EFI_KEY_DATA  KeyData2;

  DEBUG ((DEBUG_INFO, "ConfAppEntrySelect2 called \n"));
  will_return (MockSetWatchdogTimer, EFI_SUCCESS);

  expect_value (MockEnableCursor, Visible, FALSE);
  will_return (MockEnableCursor, EFI_SUCCESS);

  expect_any_count (MockSetCursorPosition, Column, 1);
  expect_any_count (MockSetCursorPosition, Row, 1);
  will_return_count (MockSetCursorPosition, EFI_SUCCESS, 1);

  will_return_always (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  KeyData1.Key.UnicodeChar = '2';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  will_return (BootOptionMgr, MainExit);
  will_return (BootOptionMgr, EFI_SUCCESS);

  KeyData2.Key.UnicodeChar = 'y';
  KeyData2.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData2);

  gResetCalled = FALSE;

  expect_value (ResetSystemWithSubtype, ResetType, EfiResetCold);
  expect_value (ResetSystemWithSubtype, ResetSubtype, &gConfAppResetGuid);

  ConfAppEntry (NULL, NULL);
  UT_ASSERT_TRUE (gResetCalled); // Assert that reset was called

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
  EFI_KEY_DATA  KeyData1;
  EFI_KEY_DATA  KeyData2;

  DEBUG ((DEBUG_INFO, "ConfAppEntrySelect3 called \n"));

  will_return (MockSetWatchdogTimer, EFI_SUCCESS);

  expect_value (MockEnableCursor, Visible, FALSE);
  will_return (MockEnableCursor, EFI_SUCCESS);

  expect_any_count (MockSetCursorPosition, Column, 1);
  expect_any_count (MockSetCursorPosition, Row, 1);
  will_return_count (MockSetCursorPosition, EFI_SUCCESS, 1);

  will_return_always (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  KeyData1.Key.UnicodeChar = '3';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  will_return (SetupConfMgr, MainExit);
  will_return (SetupConfMgr, EFI_SUCCESS);

  KeyData2.Key.UnicodeChar = 'y';
  KeyData2.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData2);

  gResetCalled = FALSE;
  expect_value (ResetSystemWithSubtype, ResetType, EfiResetCold);
  expect_value (ResetSystemWithSubtype, ResetSubtype, &gConfAppResetGuid);

  ConfAppEntry (NULL, NULL);
  UT_ASSERT_TRUE (gResetCalled); // Assert that reset was called

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
  EFI_KEY_DATA  KeyData1;
  EFI_KEY_DATA  KeyData2;
  EFI_STATUS    Status;

  DEBUG ((DEBUG_INFO, "ConfAppEntrySelect4 called \n"));
  will_return (MockSetWatchdogTimer, EFI_SUCCESS);

  expect_value (MockEnableCursor, Visible, FALSE);
  will_return (MockEnableCursor, EFI_SUCCESS);

  expect_any_count (MockSetCursorPosition, Column, 2);
  expect_any_count (MockSetCursorPosition, Row, 2);
  will_return_count (MockSetCursorPosition, EFI_SUCCESS, 2);

  will_return_always (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  KeyData1.Key.UnicodeChar = '4';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  will_return (SetupConfMgr, MainExit);
  will_return (SetupConfMgr, EFI_SUCCESS);

  KeyData2.Key.UnicodeChar = 'y';
  KeyData2.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData2);

  expect_value (MockResetSystem, ResetType, EfiResetCold);

  Status = ConfAppEntry (NULL, NULL);
  UT_ASSERT_NOT_EFI_ERROR (Status);

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
  EFI_KEY_DATA  KeyData1;
  EFI_KEY_DATA  KeyData2;
  EFI_KEY_DATA  KeyData3;

  DEBUG ((DEBUG_INFO, "ConfAppEntrySelectH called \n"));
  will_return (MockSetWatchdogTimer, EFI_SUCCESS);

  expect_value (MockEnableCursor, Visible, FALSE);
  will_return (MockEnableCursor, EFI_SUCCESS);

  expect_any_count (MockSetCursorPosition, Column, 2);
  expect_any_count (MockSetCursorPosition, Row, 2);
  will_return_count (MockSetCursorPosition, EFI_SUCCESS, 2);

  will_return_always (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  KeyData1.Key.UnicodeChar = 'h';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  KeyData2.Key.UnicodeChar = CHAR_NULL;
  KeyData2.Key.ScanCode    = SCAN_ESC;
  will_return (MockReadKey, &KeyData2);

  KeyData3.Key.UnicodeChar = 'y';
  KeyData3.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData3);

  gResetCalled = FALSE;

  expect_value (ResetSystemWithSubtype, ResetType, EfiResetCold);
  expect_value (ResetSystemWithSubtype, ResetSubtype, &gConfAppResetGuid);

  ConfAppEntry (NULL, NULL);
  UT_ASSERT_TRUE (gResetCalled); // Assert that reset was called

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
  EFI_KEY_DATA  KeyData1;
  EFI_KEY_DATA  KeyData2;

  DEBUG ((DEBUG_INFO, "ConfAppEntrySelectEsc called \n"));

  will_return (MockSetWatchdogTimer, EFI_SUCCESS);

  expect_value (MockEnableCursor, Visible, FALSE);
  will_return (MockEnableCursor, EFI_SUCCESS);

  expect_any_count (MockSetCursorPosition, Column, 1);
  expect_any_count (MockSetCursorPosition, Row, 1);
  will_return_count (MockSetCursorPosition, EFI_SUCCESS, 1);

  will_return_always (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  KeyData1.Key.UnicodeChar = CHAR_NULL;
  KeyData1.Key.ScanCode    = SCAN_ESC;
  will_return (MockReadKey, &KeyData1);

  KeyData2.Key.UnicodeChar = 'y';
  KeyData2.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData2);

  gResetCalled = FALSE;

  expect_value (ResetSystemWithSubtype, ResetType, EfiResetCold);
  expect_value (ResetSystemWithSubtype, ResetSubtype, &gConfAppResetGuid);

  ConfAppEntry (NULL, NULL);
  UT_ASSERT_TRUE (gResetCalled); // Assert that reset was called

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
  EFI_KEY_DATA  KeyData1;
  EFI_KEY_DATA  KeyData2;
  EFI_KEY_DATA  KeyData3;

  DEBUG ((DEBUG_INFO, "ConfAppEntrySelectOther called \n"));

  will_return (MockSetWatchdogTimer, EFI_SUCCESS);

  expect_value (MockEnableCursor, Visible, FALSE);
  will_return (MockEnableCursor, EFI_SUCCESS);

  expect_any_count (MockSetCursorPosition, Column, 1);
  expect_any_count (MockSetCursorPosition, Row, 1);
  will_return_count (MockSetCursorPosition, EFI_SUCCESS, 1);

  will_return_always (MockClearScreen, EFI_SUCCESS);
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

  gResetCalled = FALSE;

  expect_value (ResetSystemWithSubtype, ResetType, EfiResetCold);
  expect_value (ResetSystemWithSubtype, ResetSubtype, &gConfAppResetGuid);

  ConfAppEntry (NULL, NULL);
  UT_ASSERT_TRUE (gResetCalled); // Assert that reset was called

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConfAppEntry of ConfApp when system in MFG mode.

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
ConfAppEntryMfg (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_KEY_DATA  KeyData1;
  EFI_KEY_DATA  KeyData2;

  DEBUG ((DEBUG_INFO, "ConfAppEntryMfg called \n"));

  will_return (MockSetWatchdogTimer, EFI_SUCCESS);

  expect_value (MockEnableCursor, Visible, FALSE);
  will_return (MockEnableCursor, EFI_SUCCESS);

  expect_any_count (MockSetCursorPosition, Column, 1);
  expect_any_count (MockSetCursorPosition, Row, 1);
  will_return_count (MockSetCursorPosition, EFI_SUCCESS, 1);

  will_return_always (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  KeyData1.Key.UnicodeChar = CHAR_NULL;
  KeyData1.Key.ScanCode    = SCAN_ESC;
  will_return (MockReadKey, &KeyData1);

  KeyData2.Key.UnicodeChar = 'y';
  KeyData2.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData2);

  gResetCalled = FALSE;

  expect_value (ResetSystemWithSubtype, ResetType, EfiResetCold);
  expect_value (ResetSystemWithSubtype, ResetSubtype, &gConfAppResetGuid);

  ConfAppEntry (NULL, NULL);
  UT_ASSERT_TRUE (gResetCalled); // Assert that reset was called

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
  AddTestCase (MiscTests, "ConfApp Select 2 should go to Boot Options", "Select2", ConfAppEntrySelect2, NULL, ConfAppCleanup, NULL);
  AddTestCase (MiscTests, "ConfApp Select 3 should go to Setup Option", "Select3", ConfAppEntrySelect3, NULL, ConfAppCleanup, NULL);
  AddTestCase (MiscTests, "ConfApp Select h should reprint the options", "SelectH", ConfAppEntrySelectH, NULL, ConfAppCleanup, NULL);
  AddTestCase (MiscTests, "ConfApp Select ESC should confirm reboot", "SelectEsc", ConfAppEntrySelectEsc, NULL, ConfAppCleanup, NULL);
  AddTestCase (MiscTests, "ConfApp Select other should do nothing", "SelectOther", ConfAppEntrySelectOther, NULL, ConfAppCleanup, NULL);
  AddTestCase (MiscTests, "ConfApp entry on MFG mode should not signal ready to boot event", "EntryMFG", ConfAppEntryMfg, NULL, ConfAppCleanup, NULL);

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
