/** @file
  Unit tests of the System Information page of ConfApp module

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

#define UNIT_TEST_APP_NAME     "Conf Application System Info Unit Tests"
#define UNIT_TEST_APP_VERSION  "1.0"

#define MOCK_BUILD_TIME   L"04/01/2022"
#define MOCK_TIMER_EVENT  0xFEEDF00D

extern EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL  MockSimpleInput;
extern SysInfoState_t                     mSysInfoState;
extern UINTN                              mEndCol;
extern UINTN                              mEndRow;

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
  Mocked version of GetBuildDateStringUnicode.

  @param[out]       Buffer        The caller allocated buffer to hold the returned build
                                  date Unicode string. May be NULL with a zero Length in
                                  order to determine the length buffer needed.
  @param[in, out]   Length        On input, the count of Unicode chars available in Buffer.
                                  On output, the count of Unicode chars of data returned
                                  in Buffer, including Null-terminator.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_BUFFER_TOO_SMALL    The Length is too small for the result.
  @retval EFI_INVALID_PARAMETER   Buffer is NULL.
  @retval EFI_INVALID_PARAMETER   Length is NULL.
  @retval EFI_INVALID_PARAMETER   The Length is not 0 and Buffer is NULL.
  @retval Others                  Other implementation specific errors.
**/
EFI_STATUS
EFIAPI
GetBuildDateStringUnicode (
  OUT CHAR16 *Buffer, OPTIONAL
  IN  OUT UINTN   *Length
  )
{
  assert_non_null (Length);
  if (*Length < (sizeof (MOCK_BUILD_TIME) / sizeof (CHAR16))) {
    *Length = sizeof (MOCK_BUILD_TIME) / sizeof (CHAR16);
    return EFI_BUFFER_TOO_SMALL;
  }

  assert_non_null (Buffer);
  CopyMem (Buffer, MOCK_BUILD_TIME, sizeof (MOCK_BUILD_TIME));
  *Length = sizeof (MOCK_BUILD_TIME) / sizeof (CHAR16);

  return (EFI_STATUS)mock ();
}

/**
  Mocked version of GetTime.

  @param[out]  Time             A pointer to storage to receive a snapshot of the current time.
  @param[out]  Capabilities     An optional pointer to a buffer to receive the real time clock
                                device's capabilities.

  @retval EFI_SUCCESS           The operation completed successfully.
  @retval EFI_INVALID_PARAMETER Time is NULL.
  @retval EFI_DEVICE_ERROR      The time could not be retrieved due to hardware error.

**/
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
  Mocked version of CreateEvent.

  @param[in]   Type             The type of event to create and its mode and attributes.
  @param[in]   NotifyTpl        The task priority level of event notifications, if needed.
  @param[in]   NotifyFunction   The pointer to the event's notification function, if any.
  @param[in]   NotifyContext    The pointer to the notification function's context; corresponds to parameter
                                Context in the notification function.
  @param[out]  Event            The pointer to the newly created event if the call succeeds; undefined
                                otherwise.

  @retval EFI_SUCCESS           The event structure was created.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.
  @retval EFI_OUT_OF_RESOURCES  The event could not be allocated.

**/
EFI_STATUS
EFIAPI
MockCreateEvent (
  IN  UINT32            Type,
  IN  EFI_TPL           NotifyTpl,
  IN  EFI_EVENT_NOTIFY  NotifyFunction,
  IN  VOID              *NotifyContext,
  OUT EFI_EVENT         *Event
  )
{
  // Check that this is the right protocol being located
  assert_int_equal (Type, EVT_TIMER);
  assert_int_equal (NotifyTpl, 0);
  assert_null (NotifyFunction);
  assert_null (NotifyContext);
  assert_non_null (Event);

  *Event = (EFI_EVENT)(UINTN)MOCK_TIMER_EVENT;

  return (EFI_STATUS)mock ();
}

/**
  Mocked version of SetTimer.

  @param[in]  Event             The timer event that is to be signaled at the specified time.
  @param[in]  Type              The type of time that is specified in TriggerTime.
  @param[in]  TriggerTime       The number of 100ns units until the timer expires.
                                A TriggerTime of 0 is legal.
                                If Type is TimerRelative and TriggerTime is 0, then the timer
                                event will be signaled on the next timer tick.
                                If Type is TimerPeriodic and TriggerTime is 0, then the timer
                                event will be signaled on every timer tick.

  @retval EFI_SUCCESS           The event has been set to be signaled at the requested time.
  @retval EFI_INVALID_PARAMETER Event or Type is not valid.

**/
EFI_STATUS
EFIAPI
MockSetTimer (
  IN  EFI_EVENT        Event,
  IN  EFI_TIMER_DELAY  Type,
  IN  UINT64           TriggerTime
  )
{
  assert_ptr_equal (Event, (EFI_EVENT)(UINTN)MOCK_TIMER_EVENT);
  assert_int_equal (Type, TimerRelative);
  assert_ptr_equal (Event, (EFI_EVENT)(UINTN)MOCK_TIMER_EVENT);

  return (EFI_STATUS)mock ();
}

/**
  Mocked version of MockWaitForEvent.

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
  assert_int_equal (NumberOfEvents, 2);
  assert_int_equal (Event[0], MockSimpleInput.WaitForKeyEx);
  assert_int_equal (Event[1], (EFI_EVENT)(UINTN)MOCK_TIMER_EVENT);
  assert_non_null (Index);

  *Index = (UINTN)mock ();
  return EFI_SUCCESS;
}

/**
  Mocked version of CloseEvent.

  @param[in]  Event             The event to close.

  @retval EFI_SUCCESS           The event has been closed.

**/
EFI_STATUS
EFIAPI
MockCloseEvent (
  IN EFI_EVENT  Event
  )
{
  assert_ptr_equal (Event, (EFI_EVENT)(UINTN)MOCK_TIMER_EVENT);
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
  .CreateEvent  = MockCreateEvent,
  .SetTimer     = MockSetTimer,
  .WaitForEvent = MockWaitForEvent,
  .CloseEvent   = MockCloseEvent,
};

/**
  Clean up state machine for this page.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED                Test case cleanup succeeded.
  @retval  UNIT_TEST_ERROR_CLEANUP_FAILED  Test case cleanup failed.

**/
VOID
EFIAPI
SysInfoCleanup (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mSysInfoState = SysInfoInit;
  mEndCol       = 0;
  mEndRow       = 0;
}

/**
  Unit test for SystemInfo page when selecting ESC.

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
ConfAppSysInfoInit (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  expect_any_count (MockSetCursorPosition, Column, 2);
  expect_any_count (MockSetCursorPosition, Row, 2);
  will_return_count (MockSetCursorPosition, EFI_SUCCESS, 2);

  will_return (GetBuildDateStringUnicode, EFI_SUCCESS);

  Status = SysInfoMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SystemInfo page when selecting ESC.

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
ConfAppSysInfoSelectEsc (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS    Status;
  EFI_KEY_DATA  KeyData1;

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  expect_any_count (MockSetCursorPosition, Column, 2);
  expect_any_count (MockSetCursorPosition, Row, 2);
  will_return_count (MockSetCursorPosition, EFI_SUCCESS, 2);

  will_return (GetBuildDateStringUnicode, EFI_SUCCESS);

  Status = SysInfoMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSysInfoState, SysInfoWait);

  mSimpleTextInEx = &MockSimpleInput;

  KeyData1.Key.UnicodeChar = CHAR_NULL;
  KeyData1.Key.ScanCode    = SCAN_ESC;
  will_return (MockReadKey, &KeyData1);

  will_return (MockCreateEvent, EFI_SUCCESS);
  will_return (MockSetTimer, EFI_SUCCESS);
  will_return (MockWaitForEvent, 0);

  Status = SysInfoMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSysInfoState, SysInfoExit);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SystemInfo page when selecting others.

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
ConfAppSysInfoSelectOther (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS    Status;
  EFI_KEY_DATA  KeyData1;

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  expect_any_count (MockSetCursorPosition, Column, 2);
  expect_any_count (MockSetCursorPosition, Row, 2);
  will_return_count (MockSetCursorPosition, EFI_SUCCESS, 2);

  will_return (GetBuildDateStringUnicode, EFI_SUCCESS);

  Status = SysInfoMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSysInfoState, SysInfoWait);

  mSimpleTextInEx = &MockSimpleInput;

  KeyData1.Key.UnicodeChar = 'X';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  will_return (MockCreateEvent, EFI_SUCCESS);
  will_return (MockSetTimer, EFI_SUCCESS);
  will_return (MockWaitForEvent, 0);

  Status = SysInfoMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSysInfoState, SysInfoWait);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SystemInfo page when selecting others.

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
ConfAppSysInfoTimeRefresh (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  // Expect the prints twice
  expect_any_count (MockSetCursorPosition, Column, 4);
  expect_any_count (MockSetCursorPosition, Row, 4);
  will_return_count (MockSetCursorPosition, EFI_SUCCESS, 4);

  will_return (GetBuildDateStringUnicode, EFI_SUCCESS);

  // Initial run
  Status = SysInfoMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSysInfoState, SysInfoWait);

  mSimpleTextInEx = &MockSimpleInput;

  will_return (MockCreateEvent, EFI_SUCCESS);
  will_return (MockSetTimer, EFI_SUCCESS);
  will_return (MockWaitForEvent, 1);

  // Time out
  Status = SysInfoMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSysInfoState, SysInfoWait);

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
  AddTestCase (MiscTests, "System Info page should initialize properly", "NormalInit", ConfAppSysInfoInit, NULL, SysInfoCleanup, NULL);
  AddTestCase (MiscTests, "System Info page select Esc should go to previous menu", "SelectEsc", ConfAppSysInfoSelectEsc, NULL, SysInfoCleanup, NULL);
  AddTestCase (MiscTests, "System Info page select others should do nothing", "SelectOther", ConfAppSysInfoSelectOther, NULL, SysInfoCleanup, NULL);
  AddTestCase (MiscTests, "System Info page should auto refresh time display", "TimeRefresh", ConfAppSysInfoTimeRefresh, NULL, SysInfoCleanup, NULL);

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
