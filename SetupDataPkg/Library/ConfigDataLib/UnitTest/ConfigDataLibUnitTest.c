/** @file
  Unit tests of the ConfigDataLib instance.

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
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/ConfigDataLib.h>

#include <Library/UnitTestLib.h>

#include <Good_Config_Data.h>

#define UNIT_TEST_APP_NAME     "Conf Data Iterator Unit Tests"
#define UNIT_TEST_APP_VERSION  "1.0"

/**
  Handler function dispatched for individual tag based data.

  @param[in] ConfDataPtr  Pointer to configuration data.

  @retval EFI_INVALID_PARAMETER   Input argument is null.
  @retval EFI_SUCCESS             All p.

**/
EFI_STATUS
EFIAPI
MockSingleTagHandler (
  UINT32  Tag,
  VOID    *Buffer,
  UINTN   BufferSize
  )
{
  check_expected (Tag);
  check_expected (Buffer);
  check_expected (BufferSize);

  return (EFI_STATUS)mock ();
}

/**
  Unit test for IterateConfData with known good data.

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
IterateDataShouldComplete (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  expect_value (MockSingleTagHandler, Tag, 0xF0);
  expect_memory (MockSingleTagHandler, Buffer, mGood_Tag_0xF0, sizeof (mGood_Tag_0xF0));
  expect_value (MockSingleTagHandler, BufferSize, sizeof (mGood_Tag_0xF0));
  will_return (MockSingleTagHandler, EFI_SUCCESS);

  expect_value (MockSingleTagHandler, Tag, 0x70);
  expect_memory (MockSingleTagHandler, Buffer, mGood_Tag_0x70, sizeof (mGood_Tag_0x70));
  expect_value (MockSingleTagHandler, BufferSize, sizeof (mGood_Tag_0x70));
  will_return (MockSingleTagHandler, EFI_SUCCESS);

  expect_value (MockSingleTagHandler, Tag, 0x280);
  expect_memory (MockSingleTagHandler, Buffer, mGood_Tag_0x280, sizeof (mGood_Tag_0x280));
  expect_value (MockSingleTagHandler, BufferSize, sizeof (mGood_Tag_0x280));
  will_return (MockSingleTagHandler, EFI_SUCCESS);

  expect_value (MockSingleTagHandler, Tag, 0x180);
  expect_memory (MockSingleTagHandler, Buffer, mGood_Tag_0x180, sizeof (mGood_Tag_0x180));
  expect_value (MockSingleTagHandler, BufferSize, sizeof (mGood_Tag_0x180));
  will_return (MockSingleTagHandler, EFI_SUCCESS);

  expect_value (MockSingleTagHandler, Tag, 0x200);
  expect_memory (MockSingleTagHandler, Buffer, mGood_Tag_0x200, sizeof (mGood_Tag_0x200));
  expect_value (MockSingleTagHandler, BufferSize, sizeof (mGood_Tag_0x200));
  will_return (MockSingleTagHandler, EFI_SUCCESS);

  expect_value (MockSingleTagHandler, Tag, 0x10);
  expect_memory (MockSingleTagHandler, Buffer, mGood_Tag_0x10, sizeof (mGood_Tag_0x10));
  expect_value (MockSingleTagHandler, BufferSize, sizeof (mGood_Tag_0x10));
  will_return (MockSingleTagHandler, EFI_SUCCESS);

  expect_value (MockSingleTagHandler, Tag, 0x80);
  expect_memory (MockSingleTagHandler, Buffer, mGood_Tag_0x80, sizeof (mGood_Tag_0x80));
  expect_value (MockSingleTagHandler, BufferSize, sizeof (mGood_Tag_0x80));
  will_return (MockSingleTagHandler, EFI_SUCCESS);

  Status = IterateConfData (mKnown_Good_Config_Data, MockSingleTagHandler);
  UT_ASSERT_NOT_EFI_ERROR (Status);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for IterateConfData with null input.

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
IterateNullDataShouldFail (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  Status = IterateConfData (NULL, MockSingleTagHandler);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for IterateConfData with incorrect signature.

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
IterateBadDataShouldFail (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINT8       BadData[] = {
    0xDE, 0xAD, 0xBE, 0xEF
  };

  Status = IterateConfData (BadData, MockSingleTagHandler);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for IterateConfData without tag data should complete without dispatching.

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
IterateDataWithNoTagShouldComplete (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  CDATA_BLOB  InitData = {
    .Signature    = CFG_DATA_SIGNATURE,
    .HeaderLength = sizeof (CDATA_BLOB),
    .UsedLength   = sizeof (CDATA_BLOB),
    .TotalLength  = sizeof (CDATA_BLOB)
  };

  Status = IterateConfData (&InitData, MockSingleTagHandler);
  UT_ASSERT_NOT_EFI_ERROR (Status);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for IterateConfData with various length information.

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
IterateDataWithWrongLengthShouldFail (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  CDATA_BLOB  InitData = {
    .Signature    = CFG_DATA_SIGNATURE,
    .HeaderLength = sizeof (CDATA_BLOB),
    .UsedLength   = sizeof (CDATA_BLOB),
    .TotalLength  = sizeof (CDATA_BLOB)
  };

  InitData.UsedLength = InitData.UsedLength + 1;
  Status              = IterateConfData (&InitData, MockSingleTagHandler);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  InitData.UsedLength   = InitData.UsedLength - 1;
  InitData.HeaderLength = InitData.HeaderLength + 1;
  Status                = IterateConfData (&InitData, MockSingleTagHandler);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  return UNIT_TEST_PASSED;
}

/**
  Initialze the unit test framework, suite, and unit tests for the
  ConfigDataLib and run the ConfigDataLib unit test.

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
  UNIT_TEST_SUITE_HANDLE      ConfigDataTests;

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
  // Populate the ConfigDataLib Unit Test Suite.
  //
  Status = CreateUnitTestSuite (&ConfigDataTests, Framework, "ConfigBlobBaseLib Iteration Tests", "ConfigBlobBaseLib.Iterate", NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in CreateUnitTestSuite for ConfigDataTests\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  //
  // --------------Suite-----------Description--------------Name----------Function--------Pre---Post-------------------Context-----------
  //
  AddTestCase (ConfigDataTests, "Iterate routine should succeed", "Normal", IterateDataShouldComplete, NULL, NULL, NULL);
  AddTestCase (ConfigDataTests, "Iterate routine should fail gracefully with null pointer", "Null", IterateNullDataShouldFail, NULL, NULL, NULL);
  AddTestCase (ConfigDataTests, "Iterate routine should fail gracefully with incorrect signature", "BadSignature", IterateBadDataShouldFail, NULL, NULL, NULL);
  AddTestCase (ConfigDataTests, "Iterate routine should proceed with empty tag data", "Empty", IterateDataWithNoTagShouldComplete, NULL, NULL, NULL);
  AddTestCase (ConfigDataTests, "Iterate routine should fail gracefully with incorrect length", "WrongLength", IterateDataWithWrongLengthShouldFail, NULL, NULL, NULL);

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
