/** @file
  Unit tests of the Boot Option page of ConfApp module

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
#include <Library/UefiBootManagerLib.h>

#include <Library/UnitTestLib.h>

#include "ConfApp.h"

#define UNIT_TEST_APP_NAME     "Conf Application Boot Options Unit Tests"
#define UNIT_TEST_APP_VERSION  "1.0"

extern EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL  MockSimpleInput;
extern BootOptState_t                     mBootOptState;

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
  // Not used
  ASSERT (FALSE);
  return EFI_ACCESS_DENIED;
}

/**
  State machine for secure boot page. It will react to user input from keystroke
  to set selected secure boot option or go back to previous page.

  @retval EFI_SUCCESS           This iteration of state machine proceeds successfully.
  @retval Others                Failed to wait for valid keystrokes or failed to set
                                platform key to variable service.
**/
EFI_STATUS
EFIAPI
SecureBootMgr (
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
  Mock version of EfiBootManagerGetLoadOptions.

  @param  LoadOptionCount   Returns number of entries in the array.
  @param  LoadOptionType    The type of the load option.

  @retval NULL  No load options exist.
  @retval !NULL Array of load option entries.

**/
EFI_BOOT_MANAGER_LOAD_OPTION *
EFIAPI
EfiBootManagerGetLoadOptions (
  OUT UINTN                             *LoadOptionCount,
  IN EFI_BOOT_MANAGER_LOAD_OPTION_TYPE  LoadOptionType
  )
{
  EFI_BOOT_MANAGER_LOAD_OPTION  *ret_buf;

  assert_non_null (LoadOptionCount);
  assert_int_equal (LoadOptionType, LoadOptionTypeBoot);

  *LoadOptionCount = (UINTN)mock ();
  if (*LoadOptionCount != 0) {
    ret_buf = (EFI_BOOT_MANAGER_LOAD_OPTION *)mock ();
    return AllocateCopyPool (*LoadOptionCount * sizeof (EFI_BOOT_MANAGER_LOAD_OPTION), ret_buf);
  }

  return NULL;
}

/**
  Mocked version of EfiBootManagerBoot

  @param  BootOption    Boot Option to try and boot.
                        On return, BootOption->Status contains the boot status:
                        EFI_SUCCESS     BootOption was booted
                        EFI_UNSUPPORTED BootOption isn't supported.
                        EFI_NOT_FOUND   The BootOption was not found on the system
                        Others          BootOption failed with this error status

**/
VOID
EFIAPI
EfiBootManagerBoot (
  IN  EFI_BOOT_MANAGER_LOAD_OPTION  *BootOption
  )
{
  check_expected (BootOption);
  BootOption->Status = (EFI_STATUS)mock ();
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
  Calling this function causes a system-wide reset. This sets
  all circuitry within the system to its initial state. This type of reset
  is asynchronous to system operation and operates without regard to
  cycle boundaries.

  System reset should not return, if it returns, it means the system does
  not support cold reset.
**/
VOID
EFIAPI
ResetCold (
  VOID
  )
{
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
BootOptionsCleanup (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mBootOptState = BootOptInit;
}

/**
  Unit test for BootOptions page when selecting ESC.

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
ConfAppBootOptInit (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  expect_any (MockSetCursorPosition, Column);
  expect_any (MockSetCursorPosition, Row);
  will_return (MockSetCursorPosition, EFI_SUCCESS);

  will_return (EfiBootManagerGetLoadOptions, 0);

  Status = BootOptionMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for BootOptions page when selecting ESC.

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
ConfAppBootOptSelectEsc (
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

  will_return (EfiBootManagerGetLoadOptions, 0);

  Status = BootOptionMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mBootOptState, BootOptWait);

  mSimpleTextInEx = &MockSimpleInput;

  KeyData1.Key.UnicodeChar = CHAR_NULL;
  KeyData1.Key.ScanCode    = SCAN_ESC;
  will_return (MockReadKey, &KeyData1);

  Status = BootOptionMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mBootOptState, BootOptExit);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for BootOptions page when selecting others.

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
ConfAppBootOptSelectOther (
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

  will_return (EfiBootManagerGetLoadOptions, 0);

  Status = BootOptionMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mBootOptState, BootOptWait);

  mSimpleTextInEx = &MockSimpleInput;

  KeyData1.Key.UnicodeChar = 'X';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  Status = BootOptionMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mBootOptState, BootOptWait);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for BootOptions page when selecting boot options.

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
ConfAppBootOptSelectOne (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                    Status;
  EFI_KEY_DATA                  KeyData1;
  EFI_BOOT_MANAGER_LOAD_OPTION  BootOption = {
    .Description = L"Test1",
    .Attributes  = 0xFEEDF00D
  };

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  // Expect the prints twice
  expect_any (MockSetCursorPosition, Column);
  expect_any (MockSetCursorPosition, Row);
  will_return (MockSetCursorPosition, EFI_SUCCESS);

  will_return (EfiBootManagerGetLoadOptions, 1);
  will_return (EfiBootManagerGetLoadOptions, &BootOption);

  // Initial run
  Status = BootOptionMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mBootOptState, BootOptWait);

  mSimpleTextInEx = &MockSimpleInput;

  KeyData1.Key.UnicodeChar = '1';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  Status = BootOptionMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mBootOptState, BootOptBootNow);

  expect_memory (EfiBootManagerBoot, BootOption, &BootOption, sizeof (EFI_BOOT_MANAGER_LOAD_OPTION));
  will_return (EfiBootManagerBoot, EFI_SUCCESS);

  expect_value (MockResetSystem, ResetType, EfiResetCold);

  Status = BootOptionMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for BootOptions page when selecting boot options.

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
ConfAppBootOptSelectMore (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                    Status;
  EFI_KEY_DATA                  KeyData1;
  EFI_BOOT_MANAGER_LOAD_OPTION  BootOption[2] = {
    {
      .Description = L"Test1",
      .Attributes  = 0xFEEDF00D
    },
    {
      .Description = L"Test2",
      .Attributes  = 0xBA5EBA11
    }
  };

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  // Expect the prints twice
  expect_any (MockSetCursorPosition, Column);
  expect_any (MockSetCursorPosition, Row);
  will_return (MockSetCursorPosition, EFI_SUCCESS);

  will_return (EfiBootManagerGetLoadOptions, 2);
  will_return (EfiBootManagerGetLoadOptions, BootOption);

  // Initial run
  Status = BootOptionMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mBootOptState, BootOptWait);

  mSimpleTextInEx = &MockSimpleInput;

  KeyData1.Key.UnicodeChar = '2';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  Status = BootOptionMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mBootOptState, BootOptBootNow);

  expect_memory (EfiBootManagerBoot, BootOption, &BootOption[1], sizeof (EFI_BOOT_MANAGER_LOAD_OPTION));
  will_return (EfiBootManagerBoot, EFI_SUCCESS);

  expect_value (MockResetSystem, ResetType, EfiResetCold);

  Status = BootOptionMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);

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
  AddTestCase (MiscTests, "Boot Options page should initialize properly", "NormalInit", ConfAppBootOptInit, NULL, BootOptionsCleanup, NULL);
  AddTestCase (MiscTests, "Boot Options page select Esc should go to previous menu", "SelectEsc", ConfAppBootOptSelectEsc, NULL, BootOptionsCleanup, NULL);
  AddTestCase (MiscTests, "Boot Options page select others should do nothing", "SelectOther", ConfAppBootOptSelectOther, NULL, BootOptionsCleanup, NULL);
  AddTestCase (MiscTests, "Boot Options page should boot to single option", "BootOptionSingle", ConfAppBootOptSelectOne, NULL, BootOptionsCleanup, NULL);
  AddTestCase (MiscTests, "Boot Options page should boot to multiple options", "BootOptionMultiple", ConfAppBootOptSelectMore, NULL, BootOptionsCleanup, NULL);

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
