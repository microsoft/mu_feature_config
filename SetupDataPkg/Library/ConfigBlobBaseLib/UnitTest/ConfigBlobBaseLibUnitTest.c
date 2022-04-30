/** @file
  Unit tests of the DxeResetSystemLib instance of the ResetSystemLib class

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
#include <Library/ConfigBlobBaseLib.h>

#include <Library/UnitTestLib.h>

#include <Good_Config_Data.h>

#define UNIT_TEST_APP_NAME     "Conf Data Blob Operation Unit Tests"
#define UNIT_TEST_APP_VERSION  "1.0"

/**
  Unit test for GetConfigDataSize with known good data.

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
GetConfigDataSizeShouldComplete (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UINT32  Size;

  Size = GetConfigDataSize (mKnown_Good_Config_Data);
  UT_ASSERT_EQUAL (Size, sizeof (mKnown_Good_Config_Data));

  return UNIT_TEST_PASSED;
}

/**
  Unit test for GetConfigDataSize with NULL pointer.

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
GetConfigDataSizeShouldFailOnNull (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UINT32  Size;

  Size = GetConfigDataSize (NULL);
  UT_ASSERT_EQUAL (Size, 0);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for GetConfigDataSize with bad data.

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
GetConfigDataSizeShouldFailOnBadData (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UINT32  Size;
  UINT8   BadData[] = {
    0xDE, 0xAD, 0xBE, 0xEF
  };

  Size = GetConfigDataSize (BadData);
  UT_ASSERT_EQUAL (Size, 0);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for FindConfigDataByTag with known good data.

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
FindConfigDataByTagShouldComplete (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  VOID  *Buffer;

  Buffer = FindConfigDataByTag (mKnown_Good_Config_Data, 0xF0);
  UT_ASSERT_MEM_EQUAL (Buffer, mGood_Tag_0xF0, sizeof (mGood_Tag_0xF0));

  Buffer = FindConfigDataByTag (mKnown_Good_Config_Data, 0x70);
  UT_ASSERT_MEM_EQUAL (Buffer, mGood_Tag_0x70, sizeof (mGood_Tag_0x70));

  Buffer = FindConfigDataByTag (mKnown_Good_Config_Data, 0x280);
  UT_ASSERT_MEM_EQUAL (Buffer, mGood_Tag_0x280, sizeof (mGood_Tag_0x280));

  Buffer = FindConfigDataByTag (mKnown_Good_Config_Data, 0x180);
  UT_ASSERT_MEM_EQUAL (Buffer, mGood_Tag_0x180, sizeof (mGood_Tag_0x180));

  Buffer = FindConfigDataByTag (mKnown_Good_Config_Data, 0x200);
  UT_ASSERT_MEM_EQUAL (Buffer, mGood_Tag_0x200, sizeof (mGood_Tag_0x200));

  Buffer = FindConfigDataByTag (mKnown_Good_Config_Data, 0x10);
  UT_ASSERT_MEM_EQUAL (Buffer, mGood_Tag_0x10, sizeof (mGood_Tag_0x10));

  Buffer = FindConfigDataByTag (mKnown_Good_Config_Data, 0x80);
  UT_ASSERT_MEM_EQUAL (Buffer, mGood_Tag_0x80, sizeof (mGood_Tag_0x80));

  return UNIT_TEST_PASSED;
}

/**
  Unit test for FindConfigDataByTag with null data.

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
FindConfigDataByTagShouldFailWithNull (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  VOID  *Buffer;

  Buffer = FindConfigDataByTag (NULL, 0xF0);
  UT_ASSERT_EQUAL (Buffer, NULL);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for FindConfigDataByTag with bad data.

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
FindConfigDataByTagShouldFailWithBadData (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  VOID   *Buffer;
  UINT8  BadData[] = {
    0xDE, 0xAD, 0xBE, 0xEF
  };

  Buffer = FindConfigDataByTag (BadData, 0xF0);
  UT_ASSERT_EQUAL (Buffer, NULL);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for FindConfigDataByTag with invalid tag.

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
FindConfigDataByTagShouldFailWithInvalidTag (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  VOID  *Buffer;

  Buffer = FindConfigDataByTag (mKnown_Good_Config_Data, 0x78);
  UT_ASSERT_EQUAL (Buffer, NULL);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for FindConfigHdrByTag with known good data.

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
FindConfigHdrByTagShouldComplete (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CDATA_HEADER  *Header;
  VOID          *Buffer;

  Header = FindConfigHdrByTag (mKnown_Good_Config_Data, 0xF0);
  UT_ASSERT_EQUAL (Header->Tag, 0xF0);
  Buffer = (VOID *)((UINTN)Header + sizeof (CDATA_HEADER) + sizeof (CDATA_COND) * Header->ConditionNum);
  UT_ASSERT_MEM_EQUAL (Buffer, mGood_Tag_0xF0, sizeof (mGood_Tag_0xF0));

  Header = FindConfigHdrByTag (mKnown_Good_Config_Data, 0x70);
  UT_ASSERT_EQUAL (Header->Tag, 0x70);
  Buffer = (VOID *)((UINTN)Header + sizeof (CDATA_HEADER) + sizeof (CDATA_COND) * Header->ConditionNum);
  UT_ASSERT_MEM_EQUAL (Buffer, mGood_Tag_0x70, sizeof (mGood_Tag_0x70));

  Header = FindConfigHdrByTag (mKnown_Good_Config_Data, 0x280);
  UT_ASSERT_EQUAL (Header->Tag, 0x280);
  Buffer = (VOID *)((UINTN)Header + sizeof (CDATA_HEADER) + sizeof (CDATA_COND) * Header->ConditionNum);
  UT_ASSERT_MEM_EQUAL (Buffer, mGood_Tag_0x280, sizeof (mGood_Tag_0x280));

  Header = FindConfigHdrByTag (mKnown_Good_Config_Data, 0x180);
  UT_ASSERT_EQUAL (Header->Tag, 0x180);
  Buffer = (VOID *)((UINTN)Header + sizeof (CDATA_HEADER) + sizeof (CDATA_COND) * Header->ConditionNum);
  UT_ASSERT_MEM_EQUAL (Buffer, mGood_Tag_0x180, sizeof (mGood_Tag_0x180));

  Header = FindConfigHdrByTag (mKnown_Good_Config_Data, 0x200);
  UT_ASSERT_EQUAL (Header->Tag, 0x200);
  Buffer = (VOID *)((UINTN)Header + sizeof (CDATA_HEADER) + sizeof (CDATA_COND) * Header->ConditionNum);
  UT_ASSERT_MEM_EQUAL (Buffer, mGood_Tag_0x200, sizeof (mGood_Tag_0x200));

  Header = FindConfigHdrByTag (mKnown_Good_Config_Data, 0x10);
  UT_ASSERT_EQUAL (Header->Tag, 0x10);
  Buffer = (VOID *)((UINTN)Header + sizeof (CDATA_HEADER) + sizeof (CDATA_COND) * Header->ConditionNum);
  UT_ASSERT_MEM_EQUAL (Buffer, mGood_Tag_0x10, sizeof (mGood_Tag_0x10));

  Header = FindConfigHdrByTag (mKnown_Good_Config_Data, 0x80);
  UT_ASSERT_EQUAL (Header->Tag, 0x80);
  Buffer = (VOID *)((UINTN)Header + sizeof (CDATA_HEADER) + sizeof (CDATA_COND) * Header->ConditionNum);
  UT_ASSERT_MEM_EQUAL (Buffer, mGood_Tag_0x80, sizeof (mGood_Tag_0x80));

  return UNIT_TEST_PASSED;
}

/**
  Unit test for FindConfigHdrByTag with null.

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
FindConfigHdrByTagShouldFailWithNull (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CDATA_HEADER  *Header;

  Header = FindConfigHdrByTag (NULL, 0xF0);
  UT_ASSERT_EQUAL (Header, NULL);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for FindConfigHdrByTag with bad data.

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
FindConfigHdrByTagShouldFailWithBadData (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CDATA_HEADER  *Header;
  UINT8         BadData[] = {
    0xDE, 0xAD, 0xBE, 0xEF
  };

  Header = FindConfigHdrByTag (BadData, 0xF0);
  UT_ASSERT_EQUAL (Header, NULL);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for FindConfigHdrByTag with invalid tag.

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
FindConfigHdrByTagShouldFailWithInvalidTag (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CDATA_HEADER  *Header;

  Header = FindConfigHdrByTag (mKnown_Good_Config_Data, 0x78);
  UT_ASSERT_EQUAL (Header, NULL);

  return UNIT_TEST_PASSED;
}

// /**
//   Unit test for IterateConfData with various length information.

//   @param[in]  Context    [Optional] An optional parameter that enables:
//                          1) test-case reuse with varied parameters and
//                          2) test-case re-entry for Target tests that need a
//                          reboot.  This parameter is a VOID* and it is the
//                          responsibility of the test author to ensure that the
//                          contents are well understood by all test cases that may
//                          consume it.

//   @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
//                                         case was successful.
//   @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
// **/
// UNIT_TEST_STATUS
// EFIAPI
// IterateDataWithWrongLengthShouldFail (
//   IN UNIT_TEST_CONTEXT  Context
//   )
// {
//   EFI_STATUS  Status;
//   CDATA_BLOB  InitData = {
//     .Signature = CFG_DATA_SIGNATURE,
//     .HeaderLength = sizeof (CDATA_BLOB),
//     .UsedLength = sizeof (CDATA_BLOB),
//     .TotalLength = sizeof (CDATA_BLOB)
//   };

//   InitData.UsedLength = InitData.UsedLength + 1;
//   Status = IterateConfData (&InitData, MockSingleTagHandler);
//   UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

//   InitData.UsedLength = InitData.UsedLength - 1;
//   InitData.HeaderLength = InitData.HeaderLength + 1;
//   Status = IterateConfData (&InitData, MockSingleTagHandler);
//   UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

//   return UNIT_TEST_PASSED;
// }

/**
  Initialze the unit test framework, suite, and unit tests for the
  ConfigBlobBaseLib and run the ConfigBlobBaseLib unit test.

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
  UNIT_TEST_SUITE_HANDLE      ConfigBaseTests;

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
  // Populate the ConfigBlobBaseLib Unit Test Suite.
  //
  Status = CreateUnitTestSuite (&ConfigBaseTests, Framework, "ConfigBlobBaseLib Iteration Tests", "ConfigBlobBaseLib.Iterate", NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in CreateUnitTestSuite for ConfigBaseTests\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  //
  // --------------Suite-----------Description--------------Name----------Function--------Pre---Post-------------------Context-----------
  //
  AddTestCase (ConfigBaseTests, "GetConfigDataSize should succeed with known good data", "SizeNormal", GetConfigDataSizeShouldComplete, NULL, NULL, NULL);
  AddTestCase (ConfigBaseTests, "GetConfigDataSize should fail gracefully with null pointer", "SizeNull", GetConfigDataSizeShouldFailOnNull, NULL, NULL, NULL);
  AddTestCase (ConfigBaseTests, "GetConfigDataSize should fail gracefully with null pointer", "SizeBad", GetConfigDataSizeShouldFailOnBadData, NULL, NULL, NULL);

  AddTestCase (ConfigBaseTests, "FindConfigDataByTag should succeed with known good data", "FindTagNormal", FindConfigDataByTagShouldComplete, NULL, NULL, NULL);
  AddTestCase (ConfigBaseTests, "FindConfigDataByTag should fail with null data", "FindTagNull", FindConfigDataByTagShouldFailWithNull, NULL, NULL, NULL);
  AddTestCase (ConfigBaseTests, "FindConfigDataByTag should fail with bad data", "FindTagBadData", FindConfigDataByTagShouldFailWithBadData, NULL, NULL, NULL);
  AddTestCase (ConfigBaseTests, "FindConfigDataByTag should fail with invalid tag", "FindTagInvalidTag", FindConfigDataByTagShouldFailWithInvalidTag, NULL, NULL, NULL);

  AddTestCase (ConfigBaseTests, "FindConfigHdrByTag should succeed with known good data", "FindHdrNormal", FindConfigHdrByTagShouldComplete, NULL, NULL, NULL);
  AddTestCase (ConfigBaseTests, "FindConfigHdrByTag should fail with null", "FindHdrNull", FindConfigHdrByTagShouldFailWithNull, NULL, NULL, NULL);
  AddTestCase (ConfigBaseTests, "FindConfigHdrByTag should fail with bad data", "FindHdrBadData", FindConfigHdrByTagShouldFailWithBadData, NULL, NULL, NULL);
  AddTestCase (ConfigBaseTests, "FindConfigHdrByTag should fail with invalid tag", "FindHdrInvalidTag", FindConfigHdrByTagShouldFailWithInvalidTag, NULL, NULL, NULL);

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
