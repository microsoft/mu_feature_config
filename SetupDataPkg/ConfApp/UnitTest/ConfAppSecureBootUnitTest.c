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
#include <DfciSystemSettingTypes.h>
#include <Guid/MuVarPolicyFoundationDxe.h>
#include <Protocol/VariablePolicy.h>
#include <Protocol/DfciSettingsProvider.h>

#include <Library/BaseLib.h>
#include <Library/PrintLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/ConfigDataLib.h>
#include <Library/MuSecureBootKeySelectorLib.h>

#include <Library/UnitTestLib.h>

#include "ConfApp.h"

#define UNIT_TEST_APP_NAME     "Conf Application Secure Boot Unit Tests"
#define UNIT_TEST_APP_VERSION  "1.0"

extern EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL  MockSimpleInput;
extern SecureBootState_t                  mSecBootState;
extern BOOLEAN                            mInitialized;

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
  This function will delete the secure boot keys, thus
  disabling secure boot.

  @return EFI_SUCCESS or underlying failure code.
**/
EFI_STATUS
EFIAPI
DeleteSecureBootVariables (
  VOID
  )
{
  return (EFI_STATUS)mock ();
}

/**
  Query the index of the actively used Secure Boot keys corresponds to the Secure Boot key store, if it
  can be determined.

  @retval     UINTN   Will return an index of key store or MU_SB_CONFIG_NONE if secure boot is not enabled,
                      or MU_SB_CONFIG_UNKOWN if the active key does not match anything in the key store.

**/
UINTN
EFIAPI
GetCurrentSecureBootConfig (
  VOID
  )
{
  return (UINTN)mock ();
}

/**
  Returns the status of setting secure boot keys.

  @param  [in] Index  The index of key from key stores.

  @retval Will return the status of setting secure boot variables.

**/
EFI_STATUS
EFIAPI
SetSecureBootConfig (
  IN  UINT8  Index
  )
{
  check_expected (Index);
  return (EFI_STATUS)mock ();
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
  Mock function for WaitForEvent.

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
SecureBootCleanup (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mSecBootState = SecureBootInit;
  mInitialized  = FALSE;
}

/**
  Unit test for SecureBoot page when selecting ESC.

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
ConfAppSecureBootInit (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  will_return (GetCurrentSecureBootConfig, MU_SB_CONFIG_UNKNOWN);

  will_return (MockGetVariable, 0);
  will_return (MockGetVariable, NULL);
  will_return (MockGetVariable, EFI_NOT_FOUND);

  expect_any (MockSetCursorPosition, Column);
  expect_any (MockSetCursorPosition, Row);
  will_return (MockSetCursorPosition, EFI_SUCCESS);

  Status = SecureBootMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SecureBoot page when selecting ESC.

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
ConfAppSecureBootSelectEsc (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS    Status;
  EFI_KEY_DATA  KeyData1;

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  will_return (GetCurrentSecureBootConfig, MU_SB_CONFIG_NONE);

  expect_any (MockSetCursorPosition, Column);
  expect_any (MockSetCursorPosition, Row);
  will_return (MockSetCursorPosition, EFI_SUCCESS);

  Status = SecureBootMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSecBootState, SecureBootWait);

  mSimpleTextInEx = &MockSimpleInput;

  KeyData1.Key.UnicodeChar = CHAR_NULL;
  KeyData1.Key.ScanCode    = SCAN_ESC;
  will_return (MockReadKey, &KeyData1);

  Status = SecureBootMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSecBootState, SecureBootExit);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SecureBoot page when selecting others.

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
SecureBootSelectOther (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS    Status;
  EFI_KEY_DATA  KeyData1;

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  will_return (GetCurrentSecureBootConfig, MU_SB_CONFIG_NONE);

  expect_any (MockSetCursorPosition, Column);
  expect_any (MockSetCursorPosition, Row);
  will_return (MockSetCursorPosition, EFI_SUCCESS);

  Status = SecureBootMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSecBootState, SecureBootWait);

  mSimpleTextInEx = &MockSimpleInput;

  KeyData1.Key.UnicodeChar = 'X';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  Status = SecureBootMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSecBootState, SecureBootWait);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SecureBoot page when selecting secure boot one option.

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
SecureBootSelectOne (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                Status;
  EFI_KEY_DATA              KeyData1;
  SECURE_BOOT_PAYLOAD_INFO  SBKey = {
    .SecureBootKeyName = L"Dummy Key"
  };

  mSecureBootKeys      = &SBKey;
  mSecureBootKeysCount = 1;

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  will_return (GetCurrentSecureBootConfig, MU_SB_CONFIG_NONE);

  // Expect the prints twice
  expect_any (MockSetCursorPosition, Column);
  expect_any (MockSetCursorPosition, Row);
  will_return (MockSetCursorPosition, EFI_SUCCESS);

  // Initial run
  Status = SecureBootMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSecBootState, SecureBootWait);

  mSimpleTextInEx = &MockSimpleInput;

  KeyData1.Key.UnicodeChar = '0';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  expect_value (SetSecureBootConfig, Index, 0);
  will_return (SetSecureBootConfig, EFI_SUCCESS);

  Status = SecureBootMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSecBootState, SecureBootEnroll);

  Status = SecureBootMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSecBootState, SecureBootConfChange);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SecureBoot page when selecting secure boot options.

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
ConfAppSecureBootSelectMore (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                Status;
  EFI_KEY_DATA              KeyData1;
  SECURE_BOOT_PAYLOAD_INFO  SBKey[2] = {
    { .SecureBootKeyName = L"Dummy Key 1" },
    { .SecureBootKeyName = L"Dummy Key 2" },
  };

  mSecureBootKeys      = SBKey;
  mSecureBootKeysCount = 2;

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  will_return (GetCurrentSecureBootConfig, MU_SB_CONFIG_NONE);

  // Expect the prints twice
  expect_any (MockSetCursorPosition, Column);
  expect_any (MockSetCursorPosition, Row);
  will_return (MockSetCursorPosition, EFI_SUCCESS);

  // Initial run
  Status = SecureBootMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSecBootState, SecureBootWait);

  mSimpleTextInEx = &MockSimpleInput;

  KeyData1.Key.UnicodeChar = '1';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  expect_value (SetSecureBootConfig, Index, 1);
  will_return (SetSecureBootConfig, EFI_SUCCESS);

  Status = SecureBootMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSecBootState, SecureBootEnroll);

  Status = SecureBootMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSecBootState, SecureBootConfChange);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SecureBoot page when selecting clear option.

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
ConfAppSecureBootSelectClear (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                Status;
  EFI_KEY_DATA              KeyData1;
  SECURE_BOOT_PAYLOAD_INFO  SBKey[2] = {
    { .SecureBootKeyName = L"Dummy Key 1" },
    { .SecureBootKeyName = L"Dummy Key 2" },
  };

  mSecureBootKeys      = SBKey;
  mSecureBootKeysCount = 2;

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  will_return (GetCurrentSecureBootConfig, MU_SB_CONFIG_NONE);

  // Expect the prints twice
  expect_any (MockSetCursorPosition, Column);
  expect_any (MockSetCursorPosition, Row);
  will_return (MockSetCursorPosition, EFI_SUCCESS);

  // Initial run
  Status = SecureBootMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSecBootState, SecureBootWait);

  mSimpleTextInEx = &MockSimpleInput;

  KeyData1.Key.UnicodeChar = '0' + mSecureBootKeysCount;
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  Status = SecureBootMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSecBootState, SecureBootClear);

  Status = SecureBootMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSecBootState, SecureBootWait);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SecureBoot page when selecting secure boot options after ready to boot.

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
ConfAppSecureBootPostRTB (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                Status;
  POLICY_LOCK_VAR           LockVar = PHASE_INDICATOR_SET;
  EFI_KEY_DATA              KeyData1;
  SECURE_BOOT_PAYLOAD_INFO  SBKey[2] = {
    { .SecureBootKeyName = L"Dummy Key 1" },
    { .SecureBootKeyName = L"Dummy Key 2" },
  };

  mSecureBootKeys      = SBKey;
  mSecureBootKeysCount = 2;

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  will_return (GetCurrentSecureBootConfig, MU_SB_CONFIG_UNKNOWN);

  will_return (MockGetVariable, sizeof (LockVar));
  will_return (MockGetVariable, &LockVar);
  will_return (MockGetVariable, EFI_SUCCESS);

  // Expect the prints twice
  expect_any (MockSetCursorPosition, Column);
  expect_any (MockSetCursorPosition, Row);
  will_return (MockSetCursorPosition, EFI_SUCCESS);

  // Initial run
  Status = SecureBootMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSecBootState, SecureBootWait);

  mSimpleTextInEx = &MockSimpleInput;

  KeyData1.Key.UnicodeChar = '1';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  Status = SecureBootMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSecBootState, SecureBootError);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SecureBoot page when selecting new secure boot options.

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
ConfAppSecureBootUpdateKeys (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                Status;
  EFI_KEY_DATA              KeyData1;
  SECURE_BOOT_PAYLOAD_INFO  SBKey[2] = {
    { .SecureBootKeyName = L"Dummy Key 1" },
    { .SecureBootKeyName = L"Dummy Key 2" },
  };

  mSecureBootKeys      = SBKey;
  mSecureBootKeysCount = 2;

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  will_return (GetCurrentSecureBootConfig, 0);

  will_return (MockGetVariable, 0);
  will_return (MockGetVariable, NULL);
  will_return (MockGetVariable, EFI_NOT_FOUND);

  // Expect the prints twice
  expect_any (MockSetCursorPosition, Column);
  expect_any (MockSetCursorPosition, Row);
  will_return (MockSetCursorPosition, EFI_SUCCESS);

  // Initial run
  Status = SecureBootMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSecBootState, SecureBootWait);

  mSimpleTextInEx = &MockSimpleInput;

  KeyData1.Key.UnicodeChar = '1';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  expect_value (SetSecureBootConfig, Index, 1);
  will_return (SetSecureBootConfig, EFI_SUCCESS);

  Status = SecureBootMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSecBootState, SecureBootEnroll);

  will_return (DeleteSecureBootVariables, EFI_SUCCESS);

  Status = SecureBootMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSecBootState, SecureBootConfChange);

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
  AddTestCase (MiscTests, "Secure Boot page should initialize properly", "NormalInit", ConfAppSecureBootInit, NULL, SecureBootCleanup, NULL);
  AddTestCase (MiscTests, "Secure Boot page select Esc should go to previous menu", "SelectEsc", ConfAppSecureBootSelectEsc, NULL, SecureBootCleanup, NULL);
  AddTestCase (MiscTests, "Secure Boot page select others should do nothing", "SelectOther", SecureBootSelectOther, NULL, SecureBootCleanup, NULL);
  AddTestCase (MiscTests, "Secure Boot page should change to the first selection", "SecureBootOne", SecureBootSelectOne, NULL, SecureBootCleanup, NULL);
  AddTestCase (MiscTests, "Secure Boot page should change to non-first selection", "SecureBootMore", ConfAppSecureBootSelectMore, NULL, SecureBootCleanup, NULL);
  AddTestCase (MiscTests, "Secure Boot page should change to clear enrolled settings", "SelectClear", ConfAppSecureBootSelectClear, NULL, SecureBootCleanup, NULL);
  AddTestCase (MiscTests, "Secure Boot page should block selecting options post RTB", "PostRTB", ConfAppSecureBootPostRTB, NULL, SecureBootCleanup, NULL);
  AddTestCase (MiscTests, "Secure Boot page should success updating for selected options", "UpdateKey", ConfAppSecureBootUpdateKeys, NULL, SecureBootCleanup, NULL);

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
