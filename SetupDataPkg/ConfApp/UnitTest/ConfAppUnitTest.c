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
#include <Guid/MuVarPolicyFoundationDxe.h>

#include <Library/BaseLib.h>
#include <Library/PrintLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/ConfigDataLib.h>
#include <Library/DxeServicesLib.h>

#include <Library/UnitTestLib.h>

#include <Good_Config_Data.h>
#include "ConfApp.h"

#define UNIT_TEST_APP_NAME      "Conf Application Unit Tests"
#define UNIT_TEST_APP_VERSION   "1.0"

/**
  Entrypoint of configuration app. This function holds the main state machine for
  console based user interface.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some error occurs when executing this entry point.
**/
EFI_STATUS
EFIAPI
ConfAppEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  );

// // Mocked version of setting provider register protocol instance.
// DFCI_SETTING_ACCESS_PROTOCOL mMockDfciSetting = {
//   .Set ;
//   DFCI_SETTING_ACCESS_GET      Get;
//   DFCI_SETTING_ACCESS_RESET    Reset;
// };

extern enum ConfState_t_def mConfState;

VOID
SwitchMachineState (
  IN enum ConfState_t_def state
)
{
  mConfState = state;
}

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
  SwitchMachineState ((enum ConfState_t_def)mock());
  return (EFI_STATUS)mock();
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
  SwitchMachineState ((enum ConfState_t_def)mock());
  return (EFI_STATUS)mock();
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
  SwitchMachineState ((enum ConfState_t_def)mock());
  return (EFI_STATUS)mock();
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

/**
  This helper function does everything that CreateBasicVariablePolicy() does, but also
  uses the passed in protocol to register the policy with the infrastructure.
  Does not return a buffer, does not require the caller to free anything.

  @param[in]  VariablePolicy  Pointer to a valid instance of the VariablePolicy protocol.
  @param[in]  Namespace   Pointer to an EFI_GUID for the target variable namespace that this policy will protect.
  @param[in]  Name        [Optional] If provided, a pointer to the CHAR16 array for the target variable name.
                          Otherwise, will create a policy that targets an entire namespace.
  @param[in]  MinSize     MinSize for the VariablePolicy.
  @param[in]  MaxSize     MaxSize for the VariablePolicy.
  @param[in]  AttributesMustHave    AttributesMustHave for the VariablePolicy.
  @param[in]  AttributesCantHave    AttributesCantHave for the VariablePolicy.
  @param[in]  VarStateNamespace     Pointer to the EFI_GUID for the VARIABLE_LOCK_ON_VAR_STATE_POLICY.Namespace.
  @param[in]  VarStateName          Pointer to the CHAR16 array for the VARIABLE_LOCK_ON_VAR_STATE_POLICY.Name.
  @param[in]  VarStateValue         Value for the VARIABLE_LOCK_ON_VAR_STATE_POLICY.Value.

  @retval     EFI_INVALID_PARAMETER VariablePolicy pointer is NULL.
  @retval     EFI_STATUS    Status returned by CreateBasicVariablePolicy() or RegisterVariablePolicy().

**/
EFI_STATUS
EFIAPI
RegisterVarStateVariablePolicy (
  IN        EDKII_VARIABLE_POLICY_PROTOCOL  *VariablePolicy,
  IN CONST  EFI_GUID                        *Namespace,
  IN CONST  CHAR16                          *Name OPTIONAL,
  IN        UINT32                          MinSize,
  IN        UINT32                          MaxSize,
  IN        UINT32                          AttributesMustHave,
  IN        UINT32                          AttributesCantHave,
  IN CONST  EFI_GUID                        *VarStateNamespace,
  IN CONST  CHAR16                          *VarStateName,
  IN        UINT8                           VarStateValue
  )
{
  DEBUG ((DEBUG_INFO, "%a Register for %s under %g\n", __FUNCTION__, Name, Namespace));
  assert_ptr_equal (Namespace, PcdGetPtr (PcdConfigPolicyVariableGuid));
  assert_int_equal (MaxSize, VARIABLE_POLICY_NO_MAX_SIZE);
  assert_int_equal (AttributesMustHave, CDATA_NV_VAR_ATTR);
  assert_int_equal (AttributesCantHave, (UINT32) ~CDATA_NV_VAR_ATTR);
  assert_ptr_equal (VarStateNamespace, &gMuVarPolicyDxePhaseGuid);
  assert_memory_equal (VarStateName, READY_TO_BOOT_INDICATOR_VAR_NAME, sizeof (READY_TO_BOOT_INDICATOR_VAR_NAME));
  assert_int_equal (VarStateValue, PHASE_INDICATOR_SET);

  check_expected (Name);
  check_expected (MinSize);
  return EFI_SUCCESS;
}

/**
  Returns the value of a variable.

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
  IN     CHAR16                      *VariableName,
  IN     EFI_GUID                    *VendorGuid,
  OUT    UINT32                      *Attributes     OPTIONAL,
  IN OUT UINTN                       *DataSize,
  OUT    VOID                        *Data           OPTIONAL
  )
{
  UINTN Size;
  VOID  *RetData;

  assert_non_null (VariableName);
  assert_memory_equal (VendorGuid, PcdGetPtr (PcdConfigPolicyVariableGuid), sizeof (EFI_GUID));
  assert_non_null (DataSize);

  DEBUG ((DEBUG_INFO, "%a Name: %s, GUID: %g, Size: %x\n", __FUNCTION__, VariableName, VendorGuid, *DataSize));

  check_expected (VariableName);
  Size = (UINTN)mock();
  if (Size > *DataSize) {
    *DataSize = Size;
    return EFI_BUFFER_TOO_SMALL;
  } else {
    *DataSize = Size;
    RetData = (VOID*)mock();
    CopyMem (Data, RetData, Size);
  }

  return (EFI_STATUS)mock ();
}

/**
  Sets the value of a variable.

  @param[in]  VariableName       A Null-terminated string that is the name of the vendor's variable.
                                 Each VariableName is unique for each VendorGuid. VariableName must
                                 contain 1 or more characters. If VariableName is an empty string,
                                 then EFI_INVALID_PARAMETER is returned.
  @param[in]  VendorGuid         A unique identifier for the vendor.
  @param[in]  Attributes         Attributes bitmask to set for the variable.
  @param[in]  DataSize           The size in bytes of the Data buffer. Unless the EFI_VARIABLE_APPEND_WRITE or
                                 EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS attribute is set, a size of zero
                                 causes the variable to be deleted. When the EFI_VARIABLE_APPEND_WRITE attribute is
                                 set, then a SetVariable() call with a DataSize of zero will not cause any change to
                                 the variable value (the timestamp associated with the variable may be updated however
                                 even if no new data value is provided,see the description of the
                                 EFI_VARIABLE_AUTHENTICATION_2 descriptor below. In this case the DataSize will not
                                 be zero since the EFI_VARIABLE_AUTHENTICATION_2 descriptor will be populated).
  @param[in]  Data               The contents for the variable.

  @retval EFI_SUCCESS            The firmware has successfully stored the variable and its data as
                                 defined by the Attributes.
  @retval EFI_INVALID_PARAMETER  An invalid combination of attribute bits, name, and GUID was supplied, or the
                                 DataSize exceeds the maximum allowed.
  @retval EFI_INVALID_PARAMETER  VariableName is an empty string.
  @retval EFI_OUT_OF_RESOURCES   Not enough storage is available to hold the variable and its data.
  @retval EFI_DEVICE_ERROR       The variable could not be retrieved due to a hardware error.
  @retval EFI_WRITE_PROTECTED    The variable in question is read-only.
  @retval EFI_WRITE_PROTECTED    The variable in question cannot be deleted.
  @retval EFI_SECURITY_VIOLATION The variable could not be written due to EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACESS being set,
                                 but the AuthInfo does NOT pass the validation check carried out by the firmware.

  @retval EFI_NOT_FOUND          The variable trying to be updated or deleted was not found.

**/
STATIC
EFI_STATUS
EFIAPI
MockSetVariable (
  IN  CHAR16                       *VariableName,
  IN  EFI_GUID                     *VendorGuid,
  IN  UINT32                       Attributes,
  IN  UINTN                        DataSize,
  IN  VOID                         *Data
  )
{
  assert_non_null (VariableName);
  assert_memory_equal (VendorGuid, PcdGetPtr (PcdConfigPolicyVariableGuid), sizeof (EFI_GUID));
  assert_int_equal (Attributes, CDATA_NV_VAR_ATTR);

  DEBUG ((DEBUG_INFO, "%a Name: %s\n", __FUNCTION__, VariableName));

  check_expected (VariableName);
  check_expected (DataSize);
  check_expected (Data);

  return (EFI_STATUS)mock ();
}

///
/// Mock version of the UEFI Runtime Services Table
///
EFI_RUNTIME_SERVICES  MockRuntime = {
  .GetVariable = MockGetVariable,
  .SetVariable = MockSetVariable,
};

EFI_STATUS
EFIAPI
MockLocateProtocol (
  IN  EFI_GUID *Protocol,
  IN  VOID *Registration, OPTIONAL
  OUT VOID      **Interface
  )
{
  DEBUG ((DEBUG_INFO, "%a - %g\n", __FUNCTION__, Protocol));
  // Check that this is the right protocol being located
  check_expected_ptr (Protocol);

  // Set the protcol to one of our mock protocols
  *Interface = (VOID *)mock ();

  return EFI_SUCCESS;
}

///
/// Mock version of the UEFI Boot Services Table
///
EFI_BOOT_SERVICES  MockBoot = {
  {
    EFI_BOOT_SERVICES_SIGNATURE,         // Signature
    EFI_BOOT_SERVICES_REVISION,          // Revision
    sizeof (EFI_BOOT_SERVICES),          // HeaderSize
    0,                                   // CRC32
    0
  },
  .LocateProtocol = MockLocateProtocol,  // LocateProtocol
};

// /**
//   Unit test for ColdReset () API of the ConfDataSettingProvider.

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
// ResetColdShouldIssueAColdReset (
//   IN UNIT_TEST_CONTEXT  Context
//   )
// {
//   expect_value (MockResetSystem, ResetType, EfiResetCold);
//   expect_value (MockResetSystem, ResetStatus, EFI_SUCCESS);

//   ResetCold ();

//   return UNIT_TEST_PASSED;
// }

// /**
//   Unit test for WarmReset () API of the ConfDataSettingProvider.

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
// ResetWarmShouldIssueAWarmReset (
//   IN UNIT_TEST_CONTEXT  Context
//   )
// {
//   expect_value (MockResetSystem, ResetType, EfiResetWarm);
//   expect_value (MockResetSystem, ResetStatus, EFI_SUCCESS);

//   ResetWarm ();

//   return UNIT_TEST_PASSED;
// }

// /**
//   Unit test for ResetShutdown () API of the ConfDataSettingProvider.

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
// ResetShutdownShouldIssueAShutdown (
//   IN UNIT_TEST_CONTEXT  Context
//   )
// {
//   expect_value (MockResetSystem, ResetType, EfiResetShutdown);
//   expect_value (MockResetSystem, ResetStatus, EFI_SUCCESS);

//   ResetShutdown ();

//   return UNIT_TEST_PASSED;
// }

/**
  Unit test for ConfAppEntry of ConfApp.

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
ConfAppEntryShouldTransitionState (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  Status = ConfAppEntry (NULL, NULL);
  UT_ASSERT_NOT_EFI_ERROR (Status);

  return UNIT_TEST_PASSED;
}

/**
  Initialze the unit test framework, suite, and unit tests for the
  ConfDataSettingProvider and run the ConfDataSettingProvider unit test.

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
  AddTestCase (MiscTests, "Protocol notify routine should succeed", "ProtocolNotify", ConfAppEntryShouldTransitionState, NULL, NULL, NULL);
  // AddTestCase (MiscTests, "Get Tag ID from ascii string should succeed", "GetTagNormal", GetTagIdFromDfciIdShouldComplete, NULL, NULL, NULL);
  // AddTestCase (MiscTests, "Get Tag ID from ascii string should fail on NULL pointers", "GetTagNull", GetTagIdFromDfciIdShouldFailNull, NULL, NULL, NULL);
  // AddTestCase (MiscTests, "Get Tag ID from ascii string should fail on unformatted string", "GetTagUnformatted", GetTagIdFromDfciIdShouldFailOnUnformatted, NULL, NULL, NULL);
  // AddTestCase (MiscTests, "Get Tag ID from ascii string should fail on mail formatted string", "GetTagMalformatted", GetTagIdFromDfciIdShouldFailOnMalformatted, NULL, NULL, NULL);

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
