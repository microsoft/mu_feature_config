/** @file
  Unit tests of the ConfigKnobShimLibCommon instance.

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
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UnitTestLib.h>

#include <Library/ConfigKnobShimLib.h>

#define UNIT_TEST_APP_NAME     "Config Knob Shim Dxe Lib Unit Tests"
#define UNIT_TEST_APP_VERSION  "1.0"

#define CONFIG_KNOB_GUID  {0x52d39693, 0x4f64, 0x4ee6, {0x81, 0xde, 0x45, 0x89, 0x37, 0x72, 0x78, 0x55}}

/**
  Mocked version of GetVariable.

  @param[in]       VariableName  A Null-terminated string that is the name of the vendor's
                                 variable.
  @param[in]       VendorGuid    A unique identifier for the vendor.
  @param[out]      Attributes    If not NULL, a pointer to the memory location to return the
                                 attributes bitmask for the variable.
  @param[in, out]  DataSize      On input, the size in bytes of the return Data buffer.
                                 On output the size of data returned in Data.
  @param[out]      Data          The buffer to return the contents of the variable. May be NULL
                                 with a zero DataSize in order to determine the size buffer needed.

  @retval EFI_SUCCESS            The function completed successfully.
  @retval EFI_NOT_FOUND          The variable was not found.
  @retval EFI_BUFFER_TOO_SMALL   The DataSize is too small for the result.
  @retval EFI_INVALID_PARAMETER  VariableName is NULL.
  @retval EFI_INVALID_PARAMETER  VendorGuid is NULL.
  @retval EFI_INVALID_PARAMETER  DataSize is NULL.
  @retval EFI_INVALID_PARAMETER  The DataSize is not too small and Data is NULL.
  @retval EFI_DEVICE_ERROR       The variable could not be retrieved due to a hardware error.
  @retval EFI_SECURITY_VIOLATION The variable could not be retrieved due to an authentication failure.

**/
STATIC
EFI_STATUS
EFIAPI
MockGetVariable (
  IN     CHAR16    *VariableName,
  IN     EFI_GUID  *VendorGuid,
  OUT    UINT32    *Attributes     OPTIONAL,
  IN OUT UINTN     *DataSize,
  OUT    VOID      *Data           OPTIONAL
  )
{
  UINTN  Size;
  VOID   *RetData;

  assert_non_null (VariableName);
  assert_non_null (DataSize);

  DEBUG ((DEBUG_INFO, "%a Name: %s, GUID: %g, Size: %x\n", __FUNCTION__, VariableName, VendorGuid, *DataSize));

  Size = (UINTN)mock ();
  if (Size > *DataSize) {
    *DataSize = Size;
    return EFI_BUFFER_TOO_SMALL;
  } else {
    *DataSize = Size;
    RetData   = (VOID *)mock ();
    CopyMem (Data, RetData, Size);
  }

  *Attributes = (UINT32)mock ();

  return (EFI_STATUS)mock ();
}

///
/// Mock version of the UEFI Runtime Services Table
///
EFI_RUNTIME_SERVICES  MockRuntime = {
  .GetVariable = MockGetVariable,
};

/**
  Unit test for GetConfigKnobInvalidParamTest.

  Ditch out with invalid params to initial call.

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
GetConfigKnobInvalidParamTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  EFI_GUID    *ConfigKnobGuid;
  CHAR16      *ConfigKnobName;
  VOID        *ConfigKnobData;
  UINTN       *ProfileDefaultSize;
  VOID        *ProfileDefaultValue;

  Status = GetConfigKnob (ConfigKnobGuid, ConfigKnobName, ConfigKnobData, ProfileDefaultSize, NULL);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = GetConfigKnob (ConfigKnobGuid, ConfigKnobName, ConfigKnobData, NULL, ProfileDefaultValue);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = GetConfigKnob (ConfigKnobGuid, ConfigKnobName, NULL, ProfileDefaultSize, ProfileDefaultValue);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = GetConfigKnob (ConfigKnobGuid, NULL, ConfigKnobData, ProfileDefaultSize, ProfileDefaultValue);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = GetConfigKnob (NULL, ConfigKnobName, ConfigKnobData, ProfileDefaultSize, ProfileDefaultValue);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for GetConfigKnobFromVariableStorageSucceedTest.

  Fail to find a cached config knob policy and successfully fetch config knob from
  variable storage. Then, create cached policy.

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
GetConfigKnobFromVariableStorageSucceedTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  EFI_GUID    ConfigKnobGuid  = CONFIG_KNOB_GUID;
  CHAR16      *ConfigKnobName = L"MyDeadBeefDelivery";
  UINT64      ConfigKnobData;
  UINT64      ProfileDefaultValue = 0xDEADBEEFDEADBEEF;
  UINTN       ProfileDefaultSize  = sizeof (ProfileDefaultValue);
  UINT64      PolicyData          = 0xBEEF7777BEEF7777;

  will_return (MockGetVariable, sizeof (PolicyData));
  will_return (MockGetVariable, PolicyData);
  will_return (MockGetVariable, 7);
  will_return (MockGetVariable, EFI_SUCCESS);

  Status = GetConfigKnob (&ConfigKnobGuid, ConfigKnobName, &ProfileDefaultSize, &ProfileDefaultValue);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  UT_ASSERT_EQUAL (PolicyData, ConfigKnobData);
  UT_ASSERT_EQUAL (ProfileDefaultSize, sizeof (ProfileDefaultValue));

  return UNIT_TEST_PASSED;
}

/**
  Unit test for GetConfigKnobFromVariableStorageFailTest.

  Fail to find a cached config knob policy and fail to fetch config knob from
  variable storage. Then, set the profile default value.

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
GetConfigKnobFromVariableStorageFailTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  EFI_GUID    ConfigKnobGuid  = CONFIG_KNOB_GUID;
  CHAR16      *ConfigKnobName = L"MyDeadBeefDelivery";
  UINT64      ConfigKnobData;
  UINT64      ProfileDefaultValue = 0xDEADBEEFDEADBEEF;
  UINTN       ProfileDefaultSize  = sizeof (ProfileDefaultValue);
  UINT64      PolicyData          = 0xBEEF7777BEEF7777;

  will_return (MockGetVariable, sizeof (PolicyData));
  will_return (MockGetVariable, PolicyData);
  will_return (MockGetVariable, 7);
  will_return (MockGetVariable, EFI_NOT_FOUND);

  Status = GetConfigKnob (&ConfigKnobGuid, ConfigKnobName, &ProfileDefaultSize, &ProfileDefaultValue);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  UT_ASSERT_EQUAL (PolicyData, ProfileDefaultValue);
  UT_ASSERT_EQUAL (ProfileDefaultSize, sizeof (ProfileDefaultValue));

  return UNIT_TEST_PASSED;
}

/**
  Initialize the unit test framework, suite, and unit tests for the
  ConfigKnobShimLibCommon and run the ConfigKnobShimLibCommon unit test.

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
  UNIT_TEST_SUITE_HANDLE      ConfigKnobShimLibCommon;

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
  // Populate the ConfigKnobShimLibCommon Unit Test Suite.
  //
  Status = CreateUnitTestSuite (&ConfigKnobShimLibCommon, Framework, "ConfigKnobShimLibCommon Iteration Tests", "ConfigKnobShimLibCommon.Iterate", NULL, NULL);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in CreateUnitTestSuite for ConfigKnobShimLibCommon\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  //
  // --------------Suite-----------Description--------------Name----------Function--------Pre---Post-------------------Context-----------
  //

  //
  // Failure Tests
  //
  AddTestCase (ConfigKnobShimLibCommon, "Null params should return null", "GetConfigKnobInvalidParamTest", GetConfigKnobInvalidParamTest, NULL, NULL, NULL);

  //
  // Success Tests
  //
  AddTestCase (ConfigKnobShimLibCommon, "Retrieving config from variable should succeed", "GetConfigKnobFromVariableStorageSucceedTest", GetConfigKnobFromVariableStorageSucceedTest, NULL, NULL, NULL);
  AddTestCase (ConfigKnobShimLibCommon, "Retrieving cached config policy should succeed", "GetConfigKnobFromVariableStorageFailTest", GetConfigKnobFromVariableStorageFailTest, NULL, NULL, NULL);

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
