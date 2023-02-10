/** @file
  Unit tests of the ConfigVariableListLib instance.

  Copyright (c) Microsoft Corporation.
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

  Status = RetrieveActiveConfigVarList (mKnown_Good_Generic_Profile, sizeof (mKnown_Good_Generic_Profile), &ConfigVarListPtr, &ConfigVarListCount);
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
    ConfigVarListPtr = AllocatePool (sizeof (*ConfigVarListPtr));
    UT_ASSERT_NOT_NULL (ConfigVarListPtr);

    Status = QuerySingleActiveConfigUnicodeVarList (mKnown_Good_Generic_Profile, sizeof (mKnown_Good_Generic_Profile), mKnown_Good_VarList_Names[i], ConfigVarListPtr);
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
    ConfigVarListPtr = AllocatePool (sizeof (*ConfigVarListPtr));
    UT_ASSERT_NOT_NULL (ConfigVarListPtr);

    AsciiName = AllocatePool (StrLen (mKnown_Good_VarList_Names[i]) + 1);
    UT_ASSERT_NOT_NULL (AsciiName);

    UnicodeStrToAsciiStrS (mKnown_Good_VarList_Names[i], AsciiName, StrLen (mKnown_Good_VarList_Names[i]) + 1);

    Status = QuerySingleActiveConfigAsciiVarList (mKnown_Good_Generic_Profile, sizeof (mKnown_Good_Generic_Profile), AsciiName, ConfigVarListPtr);
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

  Status = RetrieveActiveConfigVarList (NULL, 0, NULL, &ConfigVarListCount);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = RetrieveActiveConfigVarList (NULL, 0, &ConfigVarListPtr, NULL);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = RetrieveActiveConfigVarList (NULL, sizeof (mKnown_Good_Generic_Profile), &ConfigVarListPtr, &ConfigVarListCount);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = RetrieveActiveConfigVarList (mKnown_Good_Generic_Profile, 0, &ConfigVarListPtr, &ConfigVarListCount);
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

  Status = QuerySingleActiveConfigUnicodeVarList (mKnown_Good_Generic_Profile, sizeof (mKnown_Good_Generic_Profile), NULL, &ConfigVarListPtr);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = QuerySingleActiveConfigUnicodeVarList (mKnown_Good_Generic_Profile, sizeof (mKnown_Good_Generic_Profile), &UnicodeName, NULL);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = QuerySingleActiveConfigUnicodeVarList (mKnown_Good_Generic_Profile, 0, &UnicodeName, &ConfigVarListPtr);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = QuerySingleActiveConfigUnicodeVarList (NULL, sizeof (mKnown_Good_Generic_Profile), &UnicodeName, &ConfigVarListPtr);
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

  Status = QuerySingleActiveConfigAsciiVarList (mKnown_Good_Generic_Profile, sizeof (mKnown_Good_Generic_Profile), NULL, &ConfigVarListPtr);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = QuerySingleActiveConfigAsciiVarList (mKnown_Good_Generic_Profile, sizeof (mKnown_Good_Generic_Profile), &AsciiName, NULL);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = QuerySingleActiveConfigAsciiVarList (mKnown_Good_Generic_Profile, 0, &AsciiName, &ConfigVarListPtr);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = QuerySingleActiveConfigAsciiVarList (NULL, sizeof (mKnown_Good_Generic_Profile), &AsciiName, &ConfigVarListPtr);
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

  ConfigVarListPtr = AllocatePool (sizeof (*ConfigVarListPtr));
  UT_ASSERT_NOT_NULL (ConfigVarListPtr);

  UnicodeName = AllocatePool (16);
  UT_ASSERT_NOT_NULL (UnicodeName);

  AsciiStrToUnicodeStrS ("BadName", UnicodeName, 16);

  Status = QuerySingleActiveConfigUnicodeVarList (mKnown_Good_Generic_Profile, sizeof (mKnown_Good_Generic_Profile), UnicodeName, ConfigVarListPtr);
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

  ConfigVarListPtr = AllocatePool (sizeof (*ConfigVarListPtr));
  UT_ASSERT_NOT_NULL (ConfigVarListPtr);

  AsciiName = AllocatePool (8);
  UT_ASSERT_NOT_NULL (AsciiName);

  UnicodeStrToAsciiStrS (L"BadName", AsciiName, 8);

  Status = QuerySingleActiveConfigAsciiVarList (mKnown_Good_Generic_Profile, sizeof (mKnown_Good_Generic_Profile), AsciiName, ConfigVarListPtr);
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
  UINTN                  ConfigVarListCount;

  // pass in bad data, expect an assert
  // mKnown_Bad_Config_Data is not in varlist format, so it should fail
  UT_EXPECT_ASSERT_FAILURE (RetrieveActiveConfigVarList (mKnown_Bad_Config_Data, sizeof (mKnown_Bad_Config_Data), &ConfigVarListPtr, &ConfigVarListCount), NULL);

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
  UINT32                 i                 = 0;
  CHAR8                  *AsciiName        = NULL;

  // pass in bad data, expect an assert
  // mKnown_Bad_Config_Data is not in varlist format, so it should fail
  ConfigVarListPtr = AllocatePool (sizeof (*ConfigVarListPtr));
  UT_ASSERT_NOT_NULL (ConfigVarListPtr);

  AsciiName = AllocatePool (StrLen (mKnown_Good_VarList_Names[i]) + 1);
  UT_ASSERT_NOT_NULL (AsciiName);

  UnicodeStrToAsciiStrS (mKnown_Good_VarList_Names[i], AsciiName, StrLen (mKnown_Good_VarList_Names[i]) + 1);

  UT_EXPECT_ASSERT_FAILURE (QuerySingleActiveConfigAsciiVarList (mKnown_Bad_Config_Data, sizeof (mKnown_Bad_Config_Data), AsciiName, ConfigVarListPtr), NULL);

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
  UINT32                 i                 = 0;

  // pass in bad data, expect an assert
  // mKnown_Bad_Config_Data is not in varlist format, so it should fail
  ConfigVarListPtr = AllocatePool (sizeof (*ConfigVarListPtr));
  UT_ASSERT_NOT_NULL (ConfigVarListPtr);

  UT_EXPECT_ASSERT_FAILURE (QuerySingleActiveConfigUnicodeVarList (mKnown_Bad_Config_Data, sizeof (mKnown_Bad_Config_Data), mKnown_Good_VarList_Names[i], ConfigVarListPtr), NULL);

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
  UINTN                  ConfigVarListCount;
  EFI_STATUS             Status;

  Status = RetrieveActiveConfigVarList (mKnown_Good_Generic_Profile, 0, &ConfigVarListPtr, &ConfigVarListCount);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = RetrieveActiveConfigVarList (NULL, sizeof (mKnown_Good_Generic_Profile), &ConfigVarListPtr, &ConfigVarListCount);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConvertVariableListToVariableEntry.

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
ConvertVariableListToVariableEntryNormal (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CONFIG_VAR_LIST_ENTRY  ConfigVarList;
  EFI_STATUS             Status;
  UINTN                  Size;

  Size = sizeof (mKnown_Good_Generic_Profile);
  Status = ConvertVariableListToVariableEntry (mKnown_Good_Generic_Profile, &Size, &ConfigVarList);
  UT_ASSERT_NOT_EFI_ERROR (Status);

  // StrLen * 2 as we compare all bytes, not just number of Unicode chars
  UT_ASSERT_MEM_EQUAL (mKnown_Good_VarList_Names[0], ConfigVarList.Name, StrLen (mKnown_Good_VarList_Names[0]) * 2);

  UT_ASSERT_MEM_EQUAL (&mKnown_Good_Yaml_Guid, &ConfigVarList.Guid, sizeof (mKnown_Good_Yaml_Guid));
  UT_ASSERT_EQUAL (3, ConfigVarList.Attributes);

  UT_ASSERT_EQUAL (mKnown_Good_VarList_DataSizes[0], ConfigVarList.DataSize);
  UT_ASSERT_MEM_EQUAL (mKnown_Good_VarList_Entries[0], ConfigVarList.Data, ConfigVarList.DataSize);

  UT_ASSERT_EQUAL (sizeof (CONFIG_VAR_LIST_HDR) + mKnown_Good_VarList_DataSizes[0] + StrSize (mKnown_Good_VarList_Names[0]) + sizeof (EFI_GUID) + sizeof (UINT32) + sizeof (UINT32), Size);

  FreePool (ConfigVarList.Name);
  FreePool (ConfigVarList.Data);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConvertVariableListToVariableEntry for bad sized input.

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
ConvertVariableListToVariableEntryBadSize (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CONFIG_VAR_LIST_ENTRY  ConfigVarList;
  EFI_STATUS             Status;
  UINTN                  Index;
  UINTN                  Size;
  UINTN                  ExpectedSize;

  ExpectedSize = sizeof (CONFIG_VAR_LIST_HDR) + mKnown_Good_VarList_DataSizes[0] + StrSize (mKnown_Good_VarList_Names[0]) + sizeof (EFI_GUID) + sizeof (UINT32) + sizeof (UINT32);
  UT_ASSERT_EQUAL (ExpectedSize, VAR_LIST_SIZE (StrSize (mKnown_Good_VarList_Names[0]), mKnown_Good_VarList_DataSizes[0]));

  for (Index = 0; Index < ExpectedSize; Index ++) {
    Size = Index;
    Status = ConvertVariableListToVariableEntry (mKnown_Good_Generic_Profile, &Size, &ConfigVarList);
    UT_ASSERT_STATUS_EQUAL (Status, EFI_BUFFER_TOO_SMALL);
    if (Index < sizeof (CONFIG_VAR_LIST_HDR)) {
      // The input does not even cover the header, should not be able to derive this value!!!
      UT_ASSERT_NOT_EQUAL (Size, ExpectedSize);
    } else {
      UT_ASSERT_EQUAL (Size, ExpectedSize);
    }
  }

  Size = Index;
  Status = ConvertVariableListToVariableEntry (mKnown_Good_Generic_Profile, &Size, &ConfigVarList);
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (Size, ExpectedSize);

  FreePool (ConfigVarList.Name);
  FreePool (ConfigVarList.Data);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConvertVariableListToVariableEntry for bad CRCed input.

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
ConvertVariableListToVariableEntryBadCrc (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CONFIG_VAR_LIST_ENTRY  ConfigVarList;
  EFI_STATUS             Status;
  UINTN                  ExpectedSize;
  UINT8                  *Buffer;

  ExpectedSize = VAR_LIST_SIZE (StrSize (mKnown_Good_VarList_Names[0]), mKnown_Good_VarList_DataSizes[0]);
  Buffer = AllocateCopyPool (ExpectedSize, mKnown_Good_Generic_Profile);

  Buffer[ExpectedSize - 1] = Buffer[ExpectedSize - 1] + 1;

  Status = ConvertVariableListToVariableEntry (Buffer, &ExpectedSize, &ConfigVarList);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_COMPROMISED_DATA);

  FreePool (Buffer);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConvertVariableListToVariableEntry for bad CRCed input.

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
ConvertVariableListToVariableEntryNull (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CONFIG_VAR_LIST_ENTRY  ConfigVarList;
  EFI_STATUS             Status;
  UINTN                  ExpectedSize;

  ExpectedSize = 1;

  Status = ConvertVariableListToVariableEntry (NULL, &ExpectedSize, &ConfigVarList);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = ConvertVariableListToVariableEntry (mKnown_Good_Generic_Profile, NULL, &ConfigVarList);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = ConvertVariableListToVariableEntry (mKnown_Good_Generic_Profile, &ExpectedSize, NULL);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConvertVariableEntryToVariableList.

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
ConvertVariableEntryToVariableListNormal (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CONFIG_VAR_LIST_ENTRY  ConfigVarList;
  EFI_STATUS             Status;
  UINTN                  Size;
  VOID                   *Buffer;

  ConfigVarList.Attributes = 3;
  ConfigVarList.Name = mKnown_Good_VarList_Names[0];
  ConfigVarList.Data = mKnown_Good_VarList_Entries[0];
  ConfigVarList.DataSize = mKnown_Good_VarList_DataSizes[0];
  CopyMem (&ConfigVarList.Guid, &mKnown_Good_Yaml_Guid, sizeof (EFI_GUID));

  Size = VAR_LIST_SIZE (StrSize (mKnown_Good_VarList_Names[0]), mKnown_Good_VarList_DataSizes[0]);
  Buffer = AllocatePool (Size);
  Status = ConvertVariableEntryToVariableList (&ConfigVarList, Buffer, &Size);
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (Size,  VAR_LIST_SIZE (StrSize (mKnown_Good_VarList_Names[0]), mKnown_Good_VarList_DataSizes[0]));

  // StrLen * 2 as we compare all bytes, not just number of Unicode chars
  UT_ASSERT_MEM_EQUAL (Buffer, mKnown_Good_Generic_Profile, Size);

  FreePool (Buffer);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConvertVariableEntryToVariableList with bad name or data.

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
ConvertVariableEntryToVariableListBadNameData (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CONFIG_VAR_LIST_ENTRY  ConfigVarList;
  EFI_STATUS             Status;
  UINTN                  Size;
  VOID                   *Buffer;

  ConfigVarList.Attributes = 3;
  ConfigVarList.Name = NULL;
  ConfigVarList.Data = NULL;
  ConfigVarList.DataSize = mKnown_Good_VarList_DataSizes[0];
  CopyMem (&ConfigVarList.Guid, &mKnown_Good_Yaml_Guid, sizeof (EFI_GUID));

  Size = VAR_LIST_SIZE (StrSize (mKnown_Good_VarList_Names[0]), mKnown_Good_VarList_DataSizes[0]);
  Buffer = AllocatePool (Size);

  ConfigVarList.Name = NULL;
  ConfigVarList.Data = mKnown_Good_VarList_Entries[0];
  Status = ConvertVariableEntryToVariableList (&ConfigVarList, Buffer, &Size);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  ConfigVarList.Name = mKnown_Good_VarList_Names[0];
  ConfigVarList.Data = NULL;
  Status = ConvertVariableEntryToVariableList (&ConfigVarList, Buffer, &Size);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConvertVariableEntryToVariableList with bad size.

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
ConvertVariableEntryToVariableListBadSize (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CONFIG_VAR_LIST_ENTRY  ConfigVarList;
  EFI_STATUS             Status;
  UINTN                  Size;
  UINTN                  ExpectedSize;
  UINTN                  Index;
  VOID                   *Buffer;

  ConfigVarList.Attributes = 3;
  ConfigVarList.Name = mKnown_Good_VarList_Names[0];
  ConfigVarList.Data = mKnown_Good_VarList_Entries[0];
  ConfigVarList.DataSize = mKnown_Good_VarList_DataSizes[0];
  CopyMem (&ConfigVarList.Guid, &mKnown_Good_Yaml_Guid, sizeof (EFI_GUID));

  ExpectedSize = VAR_LIST_SIZE (StrSize (mKnown_Good_VarList_Names[0]), mKnown_Good_VarList_DataSizes[0]);

  // Intentionally setting it to code, it should not be touched.
  Buffer = (VOID*)ConvertVariableEntryToVariableListBadSize;

  for (Index = 0; Index < ExpectedSize; Index ++) {
    Size = Index;
    Status = ConvertVariableEntryToVariableList (&ConfigVarList, Buffer, &Size);
    UT_ASSERT_STATUS_EQUAL (Status, EFI_BUFFER_TOO_SMALL);
    UT_ASSERT_EQUAL (Size, ExpectedSize);
  }

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConvertVariableEntryToVariableList with NULL inputs.

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
ConvertVariableEntryToVariableListNull (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CONFIG_VAR_LIST_ENTRY  ConfigVarList;
  EFI_STATUS             Status;
  UINTN                  Size;
  VOID                   *Buffer;

  Size = sizeof (ConfigVarList);
  Status = ConvertVariableEntryToVariableList (&ConfigVarList, NULL, &Size);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  // Set the buffer pointer to non-NULL
  Buffer = &ConfigVarList;
  Status = ConvertVariableEntryToVariableList (NULL, Buffer, &Size);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = ConvertVariableEntryToVariableList (&ConfigVarList, Buffer, NULL);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for VariableEntry Loop Back.

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
VariableEntryLoopBack (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CONFIG_VAR_LIST_ENTRY  ConfigVarList;
  CONFIG_VAR_LIST_ENTRY  LoopBackConfigVarList;
  EFI_STATUS             Status;
  UINTN                  Size;
  VOID                   *Buffer;

  ConfigVarList.Attributes = 3;
  ConfigVarList.Name = mKnown_Good_VarList_Names[0];
  ConfigVarList.Data = mKnown_Good_VarList_Entries[0];
  ConfigVarList.DataSize = mKnown_Good_VarList_DataSizes[0];
  CopyMem (&ConfigVarList.Guid, &mKnown_Good_Yaml_Guid, sizeof (EFI_GUID));

  Size = VAR_LIST_SIZE (StrSize(ConfigVarList.Name), ConfigVarList.DataSize);
  Buffer = AllocatePool (Size);
  Status = ConvertVariableEntryToVariableList (&ConfigVarList, Buffer, &Size);
  UT_ASSERT_NOT_EFI_ERROR (Status);

  Status = ConvertVariableListToVariableEntry (Buffer, &Size, &LoopBackConfigVarList);
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (Size, VAR_LIST_SIZE (StrSize (mKnown_Good_VarList_Names[0]), mKnown_Good_VarList_DataSizes[0]));

  UT_ASSERT_EQUAL (ConfigVarList.Attributes, LoopBackConfigVarList.Attributes);
  UT_ASSERT_EQUAL (ConfigVarList.DataSize, LoopBackConfigVarList.DataSize);
  UT_ASSERT_MEM_EQUAL (&ConfigVarList.Guid, &LoopBackConfigVarList.Guid, sizeof (EFI_GUID));
  UT_ASSERT_MEM_EQUAL (ConfigVarList.Data, LoopBackConfigVarList.Data, mKnown_Good_VarList_DataSizes[0]);
  UT_ASSERT_MEM_EQUAL (ConfigVarList.Name, LoopBackConfigVarList.Name, StrSize (mKnown_Good_VarList_Names[0]));

  FreePool (LoopBackConfigVarList.Name);
  FreePool (LoopBackConfigVarList.Data);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for VariableList Loop Back.

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
VariableListLoopBack (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CONFIG_VAR_LIST_ENTRY  ConfigVarList;
  EFI_STATUS             Status;
  UINTN                  Size;
  VOID                   *Buffer;

  Size = sizeof (mKnown_Good_Generic_Profile);
  Status = ConvertVariableListToVariableEntry (mKnown_Good_Generic_Profile, &Size, &ConfigVarList);
  UT_ASSERT_NOT_EFI_ERROR (Status);

  Buffer = AllocatePool (Size);
  Status = ConvertVariableEntryToVariableList (&ConfigVarList, Buffer, &Size);
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (Size, VAR_LIST_SIZE (StrSize (mKnown_Good_VarList_Names[0]), mKnown_Good_VarList_DataSizes[0]));

  UT_ASSERT_MEM_EQUAL (mKnown_Good_Generic_Profile, Buffer, Size);

  FreePool (ConfigVarList.Name);
  FreePool (ConfigVarList.Data);

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

  // Var list to var entry
  AddTestCase (ConfigVariableListLib, "Normal conversion should succeed", "ConvertVariableListToVariableEntryNormal", ConvertVariableListToVariableEntryNormal, NULL, NULL, NULL);
  AddTestCase (ConfigVariableListLib, "Bad sized input buffer should fail", "ConvertVariableListToVariableEntryBadSize", ConvertVariableListToVariableEntryBadSize, NULL, NULL, NULL);
  AddTestCase (ConfigVariableListLib, "Bad CRCed input buffer should fail", "ConvertVariableListToVariableEntryBadCrc", ConvertVariableListToVariableEntryBadCrc, NULL, NULL, NULL);
  AddTestCase (ConfigVariableListLib, "Null inputs should fail", "ConvertVariableListToVariableEntryNull", ConvertVariableListToVariableEntryNull, NULL, NULL, NULL);

  // Var entry to var list
  AddTestCase (ConfigVariableListLib, "Normal conversion should succeed", "ConvertVariableEntryToVariableListNormal", ConvertVariableEntryToVariableListNormal, NULL, NULL, NULL);
  AddTestCase (ConfigVariableListLib, "Bad name and data should fail", "ConvertVariableEntryToVariableListBadNameData", ConvertVariableEntryToVariableListBadNameData, NULL, NULL, NULL);
  AddTestCase (ConfigVariableListLib, "Bad size should fail", "ConvertVariableEntryToVariableListBadSize", ConvertVariableEntryToVariableListBadSize, NULL, NULL, NULL);
  AddTestCase (ConfigVariableListLib, "Null inputs should fail", "ConvertVariableEntryToVariableListNull", ConvertVariableEntryToVariableListNull, NULL, NULL, NULL);

  // Var entry & var list composite
  AddTestCase (ConfigVariableListLib, "VariableEntry loop back should succeed", "VariableEntryLoopBack", VariableEntryLoopBack, NULL, NULL, NULL);
  AddTestCase (ConfigVariableListLib, "VariableList loop back should succeed", "VariableListLoopBack", VariableListLoopBack, NULL, NULL, NULL);

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
