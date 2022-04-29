/** @file
  Unit tests of the ConfDataSettingProvider module

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

#include "ConfApp.h"

#define UNIT_TEST_APP_NAME      "Conf Application System Info Unit Tests"
#define UNIT_TEST_APP_VERSION   "1.0"

#define MOCK_BUILD_TIME         L"04/01/2022"
#define MOCK_TIMER_EVENT        0xFEEDF00D

extern EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL  MockSimpleInput;
extern enum BootOptState_t_def            mBootOptState;

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
  Returns an array of load options based on the EFI variable
  L"BootOrder"/L"DriverOrder" and the L"Boot####"/L"Driver####" variables impled by it.
  #### is the hex value of the UINT16 in each BootOrder/DriverOrder entry.

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
  EFI_BOOT_MANAGER_LOAD_OPTION *ret_buf;

  assert_non_null (LoadOptionCount);
  assert_int_equal (LoadOptionType, LoadOptionTypeBoot);

  *LoadOptionCount = (UINTN)mock();
  if (*LoadOptionCount != 0) {
    ret_buf = (EFI_BOOT_MANAGER_LOAD_OPTION*)mock();
    return AllocateCopyPool (*LoadOptionCount * sizeof (EFI_BOOT_MANAGER_LOAD_OPTION), ret_buf);
  }
  return NULL;
}

/**
  Attempt to boot the EFI boot option. This routine sets L"BootCurent" and
  signals the EFI ready to boot event. If the device path for the option starts
  with a BBS device path a legacy boot is attempted. Short form device paths are
  also supported via this rountine. A device path starting with
  MEDIA_HARDDRIVE_DP, MSG_USB_WWID_DP, MSG_USB_CLASS_DP gets expaned out
  to find the first device that matches. If the BootOption Device Path
  fails the removable media boot algorithm is attempted (\EFI\BOOTIA32.EFI,
  \EFI\BOOTX64.EFI,... only one file type is tried per processor type)

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
  BootOption->Status = (EFI_STATUS)mock();
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
MockWaitForEvent (
  IN  UINTN                    NumberOfEvents,
  IN  EFI_EVENT                *Event,
  OUT UINTN                    *Index
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
SysInfoCleanup (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  mBootOptState = 0;
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
ConfAppBootOptInit (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                Status;

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
ConfAppBootOptSelectEsc (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                Status;
  EFI_KEY_DATA              KeyData1;

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  expect_any (MockSetCursorPosition, Column);
  expect_any (MockSetCursorPosition, Row);
  will_return (MockSetCursorPosition, EFI_SUCCESS);

  will_return (EfiBootManagerGetLoadOptions, 0);

  Status = BootOptionMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mBootOptState, 1);

  mSimpleTextInEx = &MockSimpleInput;

  KeyData1.Key.UnicodeChar = CHAR_NULL;
  KeyData1.Key.ScanCode    = SCAN_ESC;
  will_return (MockReadKey, &KeyData1);

  Status = BootOptionMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mBootOptState, 4);

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
ConfAppBootOptSelectOther (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                Status;
  EFI_KEY_DATA              KeyData1;

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  expect_any (MockSetCursorPosition, Column);
  expect_any (MockSetCursorPosition, Row);
  will_return (MockSetCursorPosition, EFI_SUCCESS);

  will_return (EfiBootManagerGetLoadOptions, 0);

  Status = BootOptionMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mBootOptState, 1);

  mSimpleTextInEx = &MockSimpleInput;

  KeyData1.Key.UnicodeChar = 'X';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  Status = BootOptionMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mBootOptState, 1);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SystemInfo page when selecting boot options.

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
  BASE_LIBRARY_JUMP_BUFFER      JumpBuf;
  EFI_BOOT_MANAGER_LOAD_OPTION  BootOption = {
    .Description = L"Test1",
    .Attributes = 0xFEEDF00D
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
  UT_ASSERT_EQUAL (mBootOptState, 1);

  mSimpleTextInEx = &MockSimpleInput;

  KeyData1.Key.UnicodeChar = '1';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  Status = BootOptionMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mBootOptState, 2);

  expect_memory (EfiBootManagerBoot, BootOption, &BootOption, sizeof (EFI_BOOT_MANAGER_LOAD_OPTION));
  will_return (EfiBootManagerBoot, EFI_SUCCESS);

  will_return (ResetCold, &JumpBuf);

  if (!SetJump (&JumpBuf)) {
    BootOptionMgr ();
  }

  return UNIT_TEST_PASSED;
}


/**
  Unit test for SystemInfo page when selecting boot options.

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
  BASE_LIBRARY_JUMP_BUFFER      JumpBuf;
  EFI_BOOT_MANAGER_LOAD_OPTION  BootOption[2] = {
    {
      .Description = L"Test1",
      .Attributes = 0xFEEDF00D
    },
    {
      .Description = L"Test2",
      .Attributes = 0xBA5EBA11
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
  UT_ASSERT_EQUAL (mBootOptState, 1);

  mSimpleTextInEx = &MockSimpleInput;

  KeyData1.Key.UnicodeChar = '2';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  Status = BootOptionMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mBootOptState, 2);

  expect_memory (EfiBootManagerBoot, BootOption, &BootOption[1], sizeof (EFI_BOOT_MANAGER_LOAD_OPTION));
  will_return (EfiBootManagerBoot, EFI_SUCCESS);

  will_return (ResetCold, &JumpBuf);

  if (!SetJump (&JumpBuf)) {
    BootOptionMgr ();
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
  AddTestCase (MiscTests, "Boot Options page should initialize properly", "NormalInit", ConfAppBootOptInit, NULL, SysInfoCleanup, NULL);
  AddTestCase (MiscTests, "Boot Options page select Esc should go to previous menu", "SelectEsc", ConfAppBootOptSelectEsc, NULL, SysInfoCleanup, NULL);
  AddTestCase (MiscTests, "Boot Options page select others should do nothing", "SelectOther", ConfAppBootOptSelectOther, NULL, SysInfoCleanup, NULL);
  AddTestCase (MiscTests, "Boot Options page should boot to single option", "BootOptionSingle", ConfAppBootOptSelectOne, NULL, SysInfoCleanup, NULL);
  AddTestCase (MiscTests, "Boot Options page should boot to multiple options", "BootOptionMultiple", ConfAppBootOptSelectMore, NULL, SysInfoCleanup, NULL);

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
