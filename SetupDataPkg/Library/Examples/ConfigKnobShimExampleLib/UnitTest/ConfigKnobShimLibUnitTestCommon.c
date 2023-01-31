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
#include <Ppi/ReadOnlyVariable2.h>

#include <MockPeiServicesLib.h>

// include the c file to be able to unit test static function
#include "ConfigKnobShimLibCommon.c"

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

  if (Attributes != NULL) {
    *Attributes = (UINT32)mock ();
  }

  return (EFI_STATUS)mock ();
}

/**
  This service retrieves a variable's value using its name and GUID.

  Read the specified variable from the UEFI variable store. If the Data
  buffer is too small to hold the contents of the variable,
  the error EFI_BUFFER_TOO_SMALL is returned and DataSize is set to the
  required buffer size to obtain the data.

  @param  This                  A pointer to this instance of the EFI_PEI_READ_ONLY_VARIABLE2_PPI.
  @param  VariableName          A pointer to a null-terminated string that is the variable's name.
  @param  VariableGuid          A pointer to an EFI_GUID that is the variable's GUID. The combination of
                                VariableGuid and VariableName must be unique.
  @param  Attributes            If non-NULL, on return, points to the variable's attributes.
  @param  DataSize              On entry, points to the size in bytes of the Data buffer.
                                On return, points to the size of the data returned in Data.
  @param  Data                  Points to the buffer which will hold the returned variable value.
                                May be NULL with a zero DataSize in order to determine the size of the buffer needed.

  @retval EFI_SUCCESS           The variable was read successfully.
  @retval EFI_NOT_FOUND         The variable was not found.
  @retval EFI_BUFFER_TOO_SMALL  The DataSize is too small for the resulting data.
                                DataSize is updated with the size required for
                                the specified variable.
  @retval EFI_INVALID_PARAMETER VariableName, VariableGuid, DataSize or Data is NULL.
  @retval EFI_DEVICE_ERROR      The variable could not be retrieved because of a device error.

**/
EFI_STATUS
EFIAPI
MockEfiPeiGetVariable2 (
  IN CONST  EFI_PEI_READ_ONLY_VARIABLE2_PPI  *This,
  IN CONST  CHAR16                           *VariableName,
  IN CONST  EFI_GUID                         *VariableGuid,
  OUT       UINT32                           *Attributes,
  IN OUT    UINTN                            *DataSize,
  OUT       VOID                             *Data OPTIONAL
  )
{
  return MockGetVariable ((CHAR16 *)VariableName, (EFI_GUID *)VariableGuid, Attributes, DataSize, Data);
}

///
/// Mock version of the UEFI Runtime Services Table
///
EFI_RUNTIME_SERVICES  MockRuntime = {
  .GetVariable = MockGetVariable,
};

///
/// Mock version of the Pei Services Table
///
EFI_PEI_READ_ONLY_VARIABLE2_PPI  MockVariablePpi = {
  .GetVariable = MockEfiPeiGetVariable2,
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
  EFI_GUID    ConfigKnobGuid  = CONFIG_KNOB_GUID;
  CHAR16      *ConfigKnobName = L"MyDeadBeefDelivery";
  UINT64      ConfigKnobData;
  UINTN       ProfileDefaultSize  = sizeof (ConfigKnobData);
  UINT64      ProfileDefaultValue = 0xDEAD7777DEAD7777;

  Status = GetConfigKnob (&ConfigKnobGuid, ConfigKnobName, (VOID *)&ConfigKnobData, ProfileDefaultSize, NULL);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = GetConfigKnob (&ConfigKnobGuid, ConfigKnobName, (VOID *)&ConfigKnobData, 0, &ProfileDefaultValue);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = GetConfigKnob (&ConfigKnobGuid, ConfigKnobName, NULL, ProfileDefaultSize, &ProfileDefaultValue);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = GetConfigKnob (&ConfigKnobGuid, NULL, (VOID *)&ConfigKnobData, ProfileDefaultSize, &ProfileDefaultValue);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = GetConfigKnob (NULL, ConfigKnobName, (VOID *)&ConfigKnobData, ProfileDefaultSize, &ProfileDefaultValue);
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
  PPI_STATUS  PpiStatus           = { .Ppi = &MockVariablePpi, .Status = EFI_SUCCESS };

  // PEI only. Don't fail test for the DXE code, so that we can keep the unit test common
  will_return_maybe (PeiServicesLocatePpi, &PpiStatus);

  will_return (MockGetVariable, sizeof (PolicyData));
  will_return (MockGetVariable, &PolicyData);
  will_return (MockGetVariable, EFI_SUCCESS);

  Status = GetConfigKnob (&ConfigKnobGuid, ConfigKnobName, (VOID *)&ConfigKnobData, ProfileDefaultSize, &ProfileDefaultValue);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  UT_ASSERT_EQUAL (PolicyData, ConfigKnobData);

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
  PPI_STATUS  PpiStatus           = { .Ppi = &MockVariablePpi, .Status = EFI_SUCCESS };

  // PEI only. Don't fail test for the DXE code, so that we can keep the unit test common
  will_return_maybe (PeiServicesLocatePpi, &PpiStatus);

  will_return (MockGetVariable, sizeof (PolicyData));
  will_return (MockGetVariable, &PolicyData);
  will_return (MockGetVariable, EFI_NOT_FOUND);

  Status = GetConfigKnob (&ConfigKnobGuid, ConfigKnobName, (VOID *)&ConfigKnobData, ProfileDefaultSize, &ProfileDefaultValue);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  UT_ASSERT_EQUAL (ConfigKnobData, ProfileDefaultValue);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for GetConfigKnobFromVariableStorageFailSizeTest.

  Fail to find a cached config knob policy and succeed to fetch config knob from
  variable storage. Fail to match variable size with profile default size. Then, set the profile default value.

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
GetConfigKnobFromVariableStorageFailSizeTest (
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
  PPI_STATUS  PpiStatus           = { .Ppi = &MockVariablePpi, .Status = EFI_SUCCESS };

  // PEI only. Don't fail test for the DXE code, so that we can keep the unit test common
  will_return_maybe (PeiServicesLocatePpi, &PpiStatus);

  will_return (MockGetVariable, sizeof (UINT32));
  will_return (MockGetVariable, &PolicyData);
  will_return (MockGetVariable, EFI_SUCCESS);

  Status = GetConfigKnob (&ConfigKnobGuid, ConfigKnobName, (VOID *)&ConfigKnobData, ProfileDefaultSize, &ProfileDefaultValue);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  UT_ASSERT_EQUAL (ConfigKnobData, ProfileDefaultValue);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for GetConfigKnobFromVariableStorageFailPpiTest.

  Fail to fetch config knob from variable storage. Fail to match variable size with profile default size. Then, set the
  profile default value.

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
GetConfigKnobFromVariableStorageFailPpiTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  EFI_GUID    ConfigKnobGuid  = CONFIG_KNOB_GUID;
  CHAR16      *ConfigKnobName = L"MyDeadBeefDelivery";
  UINT64      ConfigKnobData;
  UINT64      ProfileDefaultValue = 0xDEADBEEFDEADBEEF;
  UINTN       ProfileDefaultSize  = sizeof (ProfileDefaultValue);
  PPI_STATUS  PpiStatus           = { .Ppi = NULL, .Status = EFI_NOT_FOUND };

  // PEI only. Don't fail test for the DXE code, so that we can keep the unit test common
  will_return_maybe (PeiServicesLocatePpi, &PpiStatus);

  // in this case, DXE only, as PEI failed to find variable service
  will_return_maybe (MockGetVariable, EFI_NOT_FOUND);

  Status = GetConfigKnob (&ConfigKnobGuid, ConfigKnobName, (VOID *)&ConfigKnobData, ProfileDefaultSize, &ProfileDefaultValue);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  UT_ASSERT_EQUAL (ConfigKnobData, ProfileDefaultValue);

  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
GetConfigKnobFromGeneratedNotInStorageTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  PPI_STATUS  PpiStatus = { .Ppi = NULL, .Status = EFI_NOT_FOUND };

  // PEI only. Don't fail test for the DXE code, so that we can keep the unit test common
  will_return_maybe (PeiServicesLocatePpi, &PpiStatus);

  // in this case, DXE only, as PEI failed to find variable service
  will_return_maybe (MockGetVariable, EFI_NOT_FOUND);

  // Call the generated function
  INT16  value = ConfigGetk_int16_t_d1000 ();

  // Expect the default value
  UT_ASSERT_EQUAL (value, 1000);

  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
GetConfigKnobFromGeneratedSucceedTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UINT16      OverrideValue = 500;
  PPI_STATUS  PpiStatus     = { .Ppi = &MockVariablePpi, .Status = EFI_SUCCESS };

  // PEI only. Don't fail test for the DXE code, so that we can keep the unit test common
  will_return_maybe (PeiServicesLocatePpi, &PpiStatus);

  will_return (MockGetVariable, sizeof (OverrideValue));
  will_return (MockGetVariable, &OverrideValue);
  will_return (MockGetVariable, EFI_SUCCESS);

  // Call the generated function
  INT16  value = ConfigGetk_int16_t_d1000 ();

  // Expect the overridden value
  UT_ASSERT_EQUAL (value, OverrideValue);

  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
GetConfigKnobFromGeneratedOutOfRangeTest (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  INT16       OverrideValue = -600;
  PPI_STATUS  PpiStatus     = { .Ppi = &MockVariablePpi, .Status = EFI_SUCCESS };

  // PEI only. Don't fail test for the DXE code, so that we can keep the unit test common
  will_return_maybe (PeiServicesLocatePpi, &PpiStatus);

  will_return (MockGetVariable, sizeof (OverrideValue));
  will_return (MockGetVariable, &OverrideValue);
  will_return (MockGetVariable, EFI_SUCCESS);

  // Call the generated function
  INT16  value = ConfigGetk_int16_t_d1000_minn500 ();

  // Expect the value to be the default (since the override is out of range)
  UT_ASSERT_EQUAL (value, 1000);

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
  AddTestCase (ConfigKnobShimLibCommon, "Retrieving default profile value should succeed", "GetConfigKnobFromVariableStorageFailTest", GetConfigKnobFromVariableStorageFailTest, NULL, NULL, NULL);
  AddTestCase (ConfigKnobShimLibCommon, "Retrieving default profile value should succeed", "GetConfigKnobFromVariableStorageFailSizeTest", GetConfigKnobFromVariableStorageFailSizeTest, NULL, NULL, NULL);
  AddTestCase (ConfigKnobShimLibCommon, "Retrieving default profile value should succeed", "GetConfigKnobFromVariableStorageFailPpiTest", GetConfigKnobFromVariableStorageFailPpiTest, NULL, NULL, NULL);
  AddTestCase (ConfigKnobShimLibCommon, "Retrieving non-overridden value using generated header should succeed (with default)", "GetConfigKnobFromGeneratedNotInStorageTest", GetConfigKnobFromGeneratedNotInStorageTest, NULL, NULL, NULL);
  AddTestCase (ConfigKnobShimLibCommon, "Retrieving overridden value using generated header should succeed", "GetConfigKnobFromGeneratedSucceedTest", GetConfigKnobFromGeneratedSucceedTest, NULL, NULL, NULL);
  AddTestCase (ConfigKnobShimLibCommon, "Retrieving an out of range overridden value using generated header should return default", "GetConfigKnobFromGeneratedOutOfRangeTest", GetConfigKnobFromGeneratedOutOfRangeTest, NULL, NULL, NULL);

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
