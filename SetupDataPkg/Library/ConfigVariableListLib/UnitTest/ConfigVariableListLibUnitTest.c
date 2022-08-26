/** @file
  Unit tests of the ConfigVariableListLib instance.

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
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <PiDxe.h>
#include <Library/DxeServicesLib.h>
#include <Library/ConfigVariableListLib.h>

#include <Library/UnitTestLib.h>
#include <Good_Config_Data.h>

#define UNIT_TEST_APP_NAME     "Conf Variable List Lib Unit Tests"
#define UNIT_TEST_APP_VERSION  "1.0"

/**
  Mocked version of GetSectionFromAnyFv.

  @param  NameGuid             A pointer to to the FFS filename GUID to search for
                               within any of the firmware volumes in the platform.
  @param  SectionType          Indicates the FFS section type to search for within
                               the FFS file specified by NameGuid.
  @param  SectionInstance      Indicates which section instance within the FFS file
                               specified by NameGuid to retrieve.
  @param  Buffer               On output, a pointer to a callee allocated buffer
                               containing the FFS file section that was found.
                               Is it the caller's responsibility to free this buffer
                               using FreePool().
  @param  Size                 On output, a pointer to the size, in bytes, of Buffer.

  @retval  EFI_SUCCESS          The specified FFS section was returned.
  @retval  EFI_NOT_FOUND        The specified FFS section could not be found.
  @retval  EFI_OUT_OF_RESOURCES There are not enough resources available to
                                retrieve the matching FFS section.
  @retval  EFI_DEVICE_ERROR     The FFS section could not be retrieves due to a
                                device error.
  @retval  EFI_ACCESS_DENIED    The FFS section could not be retrieves because the
                                firmware volume that
                                contains the matching FFS section does not allow reads.
**/
EFI_STATUS
EFIAPI
GetSectionFromAnyFv (
  IN CONST  EFI_GUID          *NameGuid,
  IN        EFI_SECTION_TYPE  SectionType,
  IN        UINTN             SectionInstance,
  OUT       VOID              **Buffer,
  OUT       UINTN             *Size
  )
{
  VOID  *ret_buf;

  assert_int_equal (SectionType, EFI_SECTION_RAW);
  assert_non_null (Buffer);
  assert_non_null (Size);

  ret_buf = (VOID *)mock ();
  if (ret_buf != NULL) {
    *Size   = (UINTN)mock ();
    *Buffer = AllocateCopyPool (*Size, ret_buf);
  } else {
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}

/**
  Mock retrieving a PCD Ptr.

  Returns the pointer to the buffer of the token specified by TokenNumber.

  @param[in]  TokenNumber The PCD token number to retrieve a current value for.

  @return Returns the pointer to the token specified by TokenNumber.

**/
VOID *
EFIAPI
LibPcdGetPtr (
  IN UINTN  TokenNumber
  )
{
  return (VOID *)mock ();
}

/**
  Unit test for RetrieveActiveConfigVarList.

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
RetrieveActiveConfigVarListTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CONFIG_VAR_LIST_ENTRY  *ConfigVarListPtr  = NULL;
  UINTN                  ConfigVarListCount = 0;
  EFI_STATUS             Status;
  UINT32                 i = 0;

  will_return (GetSectionFromAnyFv, mKnown_Good_Generic_Profile);
  will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Generic_Profile));
  will_return (LibPcdGetPtr, &gZeroGuid);

  Status = RetrieveActiveConfigVarList (&ConfigVarListPtr, &ConfigVarListCount);
  UT_ASSERT_NOT_EFI_ERROR (Status);

  for ( ; i < ConfigVarListCount; i++) {
    // StrLen * 2 as we compare all bytes, not just number of Unicode chars
    UT_ASSERT_MEM_EQUAL (mKnown_Good_VarList_Names[i], ConfigVarListPtr[i].Name, StrLen (mKnown_Good_VarList_Names[i]) * 2);
    if (i < 2) {
      UT_ASSERT_MEM_EQUAL (&mKnown_Good_Yaml_Guid, &ConfigVarListPtr[i].Guid, sizeof (mKnown_Good_Yaml_Guid));
      UT_ASSERT_EQUAL (3, ConfigVarListPtr[i].Attributes);
    } else {
      // Xml part of blob
      UT_ASSERT_MEM_EQUAL (&mKnown_Good_Xml_Guid, &ConfigVarListPtr[i].Guid, sizeof (mKnown_Good_Xml_Guid));
      UT_ASSERT_EQUAL (7, ConfigVarListPtr[i].Attributes);
    }

    UT_ASSERT_EQUAL (mKnown_Good_VarList_DataSizes[i], ConfigVarListPtr[i].DataSize);
    UT_ASSERT_MEM_EQUAL (mKnown_Good_VarList_Entries[i], ConfigVarListPtr[i].Data, ConfigVarListPtr[i].DataSize);

    FreePool (ConfigVarListPtr[i].Name);
    FreePool (ConfigVarListPtr[i].Data);
  }

  FreePool (ConfigVarListPtr);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for QuerySingleActiveConfigUnicodeVarList.

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
QuerySingleActiveConfigUnicodeVarListTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CONFIG_VAR_LIST_ENTRY  *ConfigVarListPtr = NULL;
  EFI_STATUS             Status;
  UINT32                 i = 0;

  for ( ; i < 9; i++) {
    will_return (GetSectionFromAnyFv, mKnown_Good_Generic_Profile);
    will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Generic_Profile));
    will_return (LibPcdGetPtr, &gZeroGuid);

    ConfigVarListPtr = AllocatePool (sizeof (*ConfigVarListPtr));
    UT_ASSERT_NOT_NULL (ConfigVarListPtr);

    Status = QuerySingleActiveConfigUnicodeVarList (mKnown_Good_VarList_Names[i], ConfigVarListPtr);
    UT_ASSERT_NOT_EFI_ERROR (Status);

    // StrLen * 2 as we compare all bytes, not just number of Unicode chars
    UT_ASSERT_MEM_EQUAL (mKnown_Good_VarList_Names[i], ConfigVarListPtr->Name, StrLen (mKnown_Good_VarList_Names[i]) * 2);
    if (i < 2) {
      UT_ASSERT_MEM_EQUAL (&mKnown_Good_Yaml_Guid, &ConfigVarListPtr->Guid, sizeof (mKnown_Good_Yaml_Guid));
      UT_ASSERT_EQUAL (3, ConfigVarListPtr->Attributes);
    } else {
      // Xml part of blob
      UT_ASSERT_MEM_EQUAL (&mKnown_Good_Xml_Guid, &ConfigVarListPtr->Guid, sizeof (mKnown_Good_Xml_Guid));
      UT_ASSERT_EQUAL (7, ConfigVarListPtr->Attributes);
    }

    UT_ASSERT_EQUAL (mKnown_Good_VarList_DataSizes[i], ConfigVarListPtr->DataSize);
    UT_ASSERT_MEM_EQUAL (mKnown_Good_VarList_Entries[i], ConfigVarListPtr->Data, ConfigVarListPtr->DataSize);

    FreePool (ConfigVarListPtr->Name);
    FreePool (ConfigVarListPtr->Data);
    FreePool (ConfigVarListPtr);
  }

  return UNIT_TEST_PASSED;
}

/**
  Unit test for QuerySingleActiveConfigAsciiVarList.

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
QuerySingleActiveConfigAsciiVarListTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CONFIG_VAR_LIST_ENTRY  *ConfigVarListPtr = NULL;
  EFI_STATUS             Status;
  UINT32                 i          = 0;
  CHAR8                  *AsciiName = NULL;

  for ( ; i < 9; i++) {
    will_return (GetSectionFromAnyFv, mKnown_Good_Generic_Profile);
    will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Generic_Profile));
    will_return (LibPcdGetPtr, &gZeroGuid);

    ConfigVarListPtr = AllocatePool (sizeof (*ConfigVarListPtr));
    UT_ASSERT_NOT_NULL (ConfigVarListPtr);

    AsciiName = AllocatePool (StrLen (mKnown_Good_VarList_Names[i]) + 1);
    UT_ASSERT_NOT_NULL (AsciiName);

    UnicodeStrToAsciiStrS (mKnown_Good_VarList_Names[i], AsciiName, StrLen (mKnown_Good_VarList_Names[i]) + 1);

    Status = QuerySingleActiveConfigAsciiVarList (AsciiName, ConfigVarListPtr);
    UT_ASSERT_NOT_EFI_ERROR (Status);

    // StrLen * 2 as we compare all bytes, not just number of Unicode chars
    UT_ASSERT_MEM_EQUAL (mKnown_Good_VarList_Names[i], ConfigVarListPtr->Name, StrLen (mKnown_Good_VarList_Names[i]) * 2);
    if (i < 2) {
      UT_ASSERT_MEM_EQUAL (&mKnown_Good_Yaml_Guid, &ConfigVarListPtr->Guid, sizeof (mKnown_Good_Yaml_Guid));
      UT_ASSERT_EQUAL (3, ConfigVarListPtr->Attributes);
    } else {
      // Xml part of blob
      UT_ASSERT_MEM_EQUAL (&mKnown_Good_Xml_Guid, &ConfigVarListPtr->Guid, sizeof (mKnown_Good_Xml_Guid));
      UT_ASSERT_EQUAL (7, ConfigVarListPtr->Attributes);
    }

    UT_ASSERT_EQUAL (mKnown_Good_VarList_DataSizes[i], ConfigVarListPtr->DataSize);
    UT_ASSERT_MEM_EQUAL (mKnown_Good_VarList_Entries[i], ConfigVarListPtr->Data, ConfigVarListPtr->DataSize);

    FreePool (AsciiName);
    FreePool (ConfigVarListPtr->Name);
    FreePool (ConfigVarListPtr->Data);
    FreePool (ConfigVarListPtr);
  }

  return UNIT_TEST_PASSED;
}

/**
  Unit test for RetrieveActiveConfigVarListTest.

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
RetrieveActiveConfigVarListInvalidParamTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CONFIG_VAR_LIST_ENTRY  *ConfigVarListPtr;
  EFI_STATUS             Status;
  UINTN                  ConfigVarListCount;

  Status = RetrieveActiveConfigVarList (NULL, &ConfigVarListCount);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = RetrieveActiveConfigVarList (&ConfigVarListPtr, NULL);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for QuerySingleActiveConfigUnicodeVarList.

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
QuerySingleActiveConfigUnicodeVarListInvalidParamTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CONFIG_VAR_LIST_ENTRY  ConfigVarListPtr;
  EFI_STATUS             Status;
  CHAR16                 UnicodeName;

  Status = QuerySingleActiveConfigUnicodeVarList (NULL, &ConfigVarListPtr);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = QuerySingleActiveConfigUnicodeVarList (&UnicodeName, NULL);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for QuerySingleActiveConfigAsciiVarList.

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
QuerySingleActiveConfigAsciiVarListInvalidParamTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CONFIG_VAR_LIST_ENTRY  ConfigVarListPtr;
  EFI_STATUS             Status;
  CHAR8                  AsciiName;

  Status = QuerySingleActiveConfigAsciiVarList (NULL, &ConfigVarListPtr);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = QuerySingleActiveConfigAsciiVarList (&AsciiName, NULL);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for QuerySingleActiveConfigUnicodeVarList.

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
QuerySingleActiveConfigUnicodeVarListNotFoundTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CONFIG_VAR_LIST_ENTRY  *ConfigVarListPtr = NULL;
  EFI_STATUS             Status;
  CHAR16                 *UnicodeName = NULL;

  will_return (GetSectionFromAnyFv, mKnown_Good_Generic_Profile);
  will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Generic_Profile));
  will_return (LibPcdGetPtr, &gZeroGuid);

  ConfigVarListPtr = AllocatePool (sizeof (*ConfigVarListPtr));
  UT_ASSERT_NOT_NULL (ConfigVarListPtr);

  UnicodeName = AllocatePool (16);
  UT_ASSERT_NOT_NULL (UnicodeName);

  AsciiStrToUnicodeStrS ("BadName", UnicodeName, 16);

  Status = QuerySingleActiveConfigUnicodeVarList (UnicodeName, ConfigVarListPtr);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_NOT_FOUND);

  FreePool (UnicodeName);
  FreePool (ConfigVarListPtr);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for QuerySingleActiveConfigAsciiVarList.

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
QuerySingleActiveConfigAsciiVarListNotFoundTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CONFIG_VAR_LIST_ENTRY  *ConfigVarListPtr = NULL;
  EFI_STATUS             Status;
  CHAR8                  *AsciiName = NULL;

  will_return (GetSectionFromAnyFv, mKnown_Good_Generic_Profile);
  will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Generic_Profile));
  will_return (LibPcdGetPtr, &gZeroGuid);

  ConfigVarListPtr = AllocatePool (sizeof (*ConfigVarListPtr));
  UT_ASSERT_NOT_NULL (ConfigVarListPtr);

  AsciiName = AllocatePool (8);
  UT_ASSERT_NOT_NULL (AsciiName);

  UnicodeStrToAsciiStrS (L"BadName", AsciiName, 8);

  Status = QuerySingleActiveConfigAsciiVarList (AsciiName, ConfigVarListPtr);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_NOT_FOUND);

  FreePool (AsciiName);
  FreePool (ConfigVarListPtr);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for RetrieveActiveConfigVarListTest.

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
RetrieveActiveConfigVarListBadDataTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CONFIG_VAR_LIST_ENTRY  *ConfigVarListPtr;
  EFI_STATUS             Status;
  UINTN                  ConfigVarListCount;

  // pass in bad data, expect an assert
  // mKnown_Good_Config_Data is not in varlist format, so it should fail
  will_return (GetSectionFromAnyFv, mKnown_Good_Config_Data);
  will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Config_Data));
  will_return (LibPcdGetPtr, &gZeroGuid);

  UT_EXPECT_ASSERT_FAILURE (Status = RetrieveActiveConfigVarList (&ConfigVarListPtr, &ConfigVarListCount), NULL);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for QuerySingleActiveConfigAsciiVarList.

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
QuerySingleActiveConfigAsciiVarListBadDataTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CONFIG_VAR_LIST_ENTRY  *ConfigVarListPtr = NULL;
  EFI_STATUS             Status;
  UINT32                 i          = 0;
  CHAR8                  *AsciiName = NULL;

  // pass in bad data, expect an assert
  // mKnown_Good_Config_Data is not in varlist format, so it should fail
  will_return (GetSectionFromAnyFv, mKnown_Good_Config_Data);
  will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Config_Data));
  will_return (LibPcdGetPtr, &gZeroGuid);

  ConfigVarListPtr = AllocatePool (sizeof (*ConfigVarListPtr));
  UT_ASSERT_NOT_NULL (ConfigVarListPtr);

  AsciiName = AllocatePool (StrLen (mKnown_Good_VarList_Names[i]) + 1);
  UT_ASSERT_NOT_NULL (AsciiName);

  UnicodeStrToAsciiStrS (mKnown_Good_VarList_Names[i], AsciiName, StrLen (mKnown_Good_VarList_Names[i]) + 1);

  UT_EXPECT_ASSERT_FAILURE (Status = QuerySingleActiveConfigAsciiVarList (AsciiName, ConfigVarListPtr), NULL);

  FreePool (AsciiName);
  FreePool (ConfigVarListPtr);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for QuerySingleActiveConfigUnicodeVarList.

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
QuerySingleActiveConfigUnicodeVarListBadDataTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CONFIG_VAR_LIST_ENTRY  *ConfigVarListPtr = NULL;
  EFI_STATUS             Status;
  UINT32                 i = 0;

  // pass in bad data, expect an assert
  // mKnown_Good_Config_Data is not in varlist format, so it should fail
  will_return (GetSectionFromAnyFv, mKnown_Good_Config_Data);
  will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Config_Data));
  will_return (LibPcdGetPtr, &gZeroGuid);

  ConfigVarListPtr = AllocatePool (sizeof (*ConfigVarListPtr));
  UT_ASSERT_NOT_NULL (ConfigVarListPtr);

  UT_EXPECT_ASSERT_FAILURE (Status = QuerySingleActiveConfigUnicodeVarList (mKnown_Good_VarList_Names[i], ConfigVarListPtr), NULL);

  FreePool (ConfigVarListPtr);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for RetrieveActiveConfigVarListTest.

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
RetrieveActiveConfigVarListNoProfileTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CONFIG_VAR_LIST_ENTRY  *ConfigVarListPtr;
  EFI_STATUS             Status;
  UINTN                  ConfigVarListCount;

  will_return (GetSectionFromAnyFv, NULL);
  will_return (LibPcdGetPtr, &gZeroGuid);

  UT_EXPECT_ASSERT_FAILURE (Status = RetrieveActiveConfigVarList (&ConfigVarListPtr, &ConfigVarListCount), NULL);

  will_return (GetSectionFromAnyFv, mKnown_Good_Generic_Profile);
  will_return (GetSectionFromAnyFv, 0);
  will_return (LibPcdGetPtr, &gZeroGuid);

  UT_EXPECT_ASSERT_FAILURE (Status = RetrieveActiveConfigVarList (&ConfigVarListPtr, &ConfigVarListCount), NULL);

  return UNIT_TEST_PASSED;
}

/**
  Initialize the unit test framework, suite, and unit tests for the
  ConfigVariableListLib and run the ConfigVariableListLib unit test.

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
  UNIT_TEST_SUITE_HANDLE      ConfigVariableListLib;

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
  // Populate the ConfigVariableListLib Unit Test Suite.
  //
  Status = CreateUnitTestSuite (&ConfigVariableListLib, Framework, "ConfigVariableListLib Iteration Tests", "ConfigVariableListLib.Iterate", NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in CreateUnitTestSuite for ConfigVariableListLib\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  //
  // --------------Suite-----------Description--------------Name----------Function--------Pre---Post-------------------Context-----------
  //

  //
  // Success Tests
  //
  AddTestCase (ConfigVariableListLib, "Retrieve entire config should succeed", "RetrieveActiveConfigVarListTest", RetrieveActiveConfigVarListTest, NULL, NULL, NULL);
  AddTestCase (ConfigVariableListLib, "Query single Unicode config should succeed", "QuerySingleActiveConfigUnicodeVarListTest", QuerySingleActiveConfigUnicodeVarListTest, NULL, NULL, NULL);
  AddTestCase (ConfigVariableListLib, "Query single Ascii config should succeed", "QuerySingleActiveConfigAsciiVarListTest", QuerySingleActiveConfigAsciiVarListTest, NULL, NULL, NULL);

  //
  // Failure Tests
  //

  // Invalid Param
  AddTestCase (ConfigVariableListLib, "Null Param test should fail", "RetrieveActiveConfigVarListInvalidParamTest", RetrieveActiveConfigVarListInvalidParamTest, NULL, NULL, NULL);
  AddTestCase (ConfigVariableListLib, "Null Param test should fail", "QuerySingleActiveConfigUnicodeVarListInvalidParamTest", QuerySingleActiveConfigUnicodeVarListInvalidParamTest, NULL, NULL, NULL);
  AddTestCase (ConfigVariableListLib, "Null Param test should fail", "QuerySingleActiveConfigAsciiVarListInvalidParamTest", QuerySingleActiveConfigAsciiVarListInvalidParamTest, NULL, NULL, NULL);

  // Bad name
  AddTestCase (ConfigVariableListLib, "Bad name test should fail", "QuerySingleActiveConfigUnicodeVarListNotFoundTest", QuerySingleActiveConfigUnicodeVarListNotFoundTest, NULL, NULL, NULL);
  AddTestCase (ConfigVariableListLib, "Bad name test should fail", "QuerySingleActiveConfigAsciiVarListNotFoundTest", QuerySingleActiveConfigAsciiVarListNotFoundTest, NULL, NULL, NULL);

  // Bad data
  AddTestCase (ConfigVariableListLib, "Bad data test should fail", "RetrieveActiveConfigVarListBadDataTest", RetrieveActiveConfigVarListBadDataTest, NULL, NULL, NULL);
  AddTestCase (ConfigVariableListLib, "Bad data test should fail", "QuerySingleActiveConfigUnicodeVarListBadDataTest", QuerySingleActiveConfigUnicodeVarListBadDataTest, NULL, NULL, NULL);
  AddTestCase (ConfigVariableListLib, "Bad data test should fail", "QuerySingleActiveConfigAsciiVarListBadDataTest", QuerySingleActiveConfigAsciiVarListBadDataTest, NULL, NULL, NULL);

  // No Profile
  AddTestCase (ConfigVariableListLib, "No profile test should fail", "RetrieveActiveConfigVarListNoProfileTest", RetrieveActiveConfigVarListNoProfileTest, NULL, NULL, NULL);
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
