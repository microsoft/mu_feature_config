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

extern EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL  MockSimpleInput;

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

// Mocked version of DFCI_AUTHENTICATION_PROTOCOL instance.
EFI_STATUS
EFIAPI
MockGetEnrolledIdentities (
  IN CONST DFCI_AUTHENTICATION_PROTOCOL     *This,
  OUT      DFCI_IDENTITY_MASK               *EnrolledIdentities
  )
{
  assert_non_null (This);
  assert_non_null (EnrolledIdentities);

  *EnrolledIdentities = (DFCI_IDENTITY_MASK)mock();
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
MockAuthWithPW (
  IN  CONST DFCI_AUTHENTICATION_PROTOCOL  *This,
  IN  CONST CHAR16                        *Password OPTIONAL,
  IN  UINTN                                PasswordLength,
  OUT DFCI_AUTH_TOKEN                     *IdentityToken
  )
{
  assert_non_null (This);
  assert_null (Password);
  assert_int_equal (PasswordLength, 0);

  *IdentityToken = (DFCI_AUTH_TOKEN)mock();
  return EFI_SUCCESS;
}

DFCI_AUTHENTICATION_PROTOCOL MockAuthProtocol = {
  .GetEnrolledIdentities = MockGetEnrolledIdentities,
  .AuthWithPW = MockAuthWithPW,
};

extern enum ConfState_t_def mConfState;

VOID
SwitchMachineState (
  IN UINT32   State
)
{
  mConfState = State;
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
  SwitchMachineState ((UINT32)mock());
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
  SwitchMachineState ((UINT32)mock());
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
  SwitchMachineState ((UINT32)mock());
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

EFI_STATUS
EFIAPI
MockHandleProtocol (
  IN  EFI_HANDLE               Handle,
  IN  EFI_GUID                 *Protocol,
  OUT VOID                     **Interface
  )
{
  DEBUG ((DEBUG_INFO, "%a - %g\n", __FUNCTION__, Protocol));
  assert_non_null (Interface);
  assert_ptr_equal (Protocol, &gEfiSimpleTextInputExProtocolGuid);

  *Interface = &MockSimpleInput;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
MockSetWatchdogTimer (
  IN UINTN                    Timeout,
  IN UINT64                   WatchdogCode,
  IN UINTN                    DataSize,
  IN CHAR16                   *WatchdogData OPTIONAL
  )
{
  // Check that this is the right protocol being located
  assert_int_equal (Timeout, 0);
  assert_int_equal (WatchdogCode, 0);
  assert_int_equal (DataSize, 0);
  assert_null (WatchdogData);

  return (EFI_STATUS)mock();
}

// EFI_STATUS
// EFIAPI
// MockCreateEvent (
//   IN  UINT32                       Type,
//   IN  EFI_TPL                      NotifyTpl,
//   IN  EFI_EVENT_NOTIFY             NotifyFunction,
//   IN  VOID                         *NotifyContext,
//   OUT EFI_EVENT                    *Event
//   )
// {
//   return 
// }

// EFI_STATUS
// EFIAPI
// MockSetTimer (
//   IN  EFI_EVENT                Event,
//   IN  EFI_TIMER_DELAY          Type,
//   IN  UINT64                   TriggerTime
//   )
// {

// }

EFI_STATUS
EFIAPI
MockWaitForEvent (
  IN  UINTN                    NumberOfEvents,
  IN  EFI_EVENT                *Event,
  OUT UINTN                    *Index
  )
{
  assert_int_equal (NumberOfEvents, 1);
  assert_int_equal (*Event, MockSimpleInput.WaitForKeyEx);
  assert_non_null (Index);

  *Index = 0;
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
MockCloseEvent (
  IN EFI_EVENT                Event
  )
{
  assert_int_equal (Event, MockSimpleInput.WaitForKeyEx);
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
  .SetWatchdogTimer = MockSetWatchdogTimer,
  .HandleProtocol = MockHandleProtocol,
  .LocateProtocol = MockLocateProtocol,
  .WaitForEvent = MockWaitForEvent,
  .CloseEvent = MockCloseEvent,
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
  Unit test for ConfAppEntry of ConfApp when selecting 1.

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
ConfAppEntrySelect1 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_KEY_DATA              KeyData1;
  EFI_KEY_DATA              KeyData2;
  BASE_LIBRARY_JUMP_BUFFER  JumpBuf;

  will_return (MockSetWatchdogTimer, EFI_SUCCESS);

  expect_value (MockEnableCursor, Visible, FALSE);
  will_return (MockEnableCursor, EFI_SUCCESS);

  expect_value (MockLocateProtocol, Protocol, &gDfciSettingAccessProtocolGuid);
  will_return (MockLocateProtocol, NULL);

  expect_value (MockLocateProtocol, Protocol, &gDfciAuthenticationProtocolGuid);
  will_return (MockLocateProtocol, &MockAuthProtocol);

  will_return (MockAuthWithPW, 0xFEEDF00D);
  will_return (MockGetEnrolledIdentities, DFCI_IDENTITY_LOCAL);

  expect_any_count (MockSetCursorPosition, Column, 1);
  expect_any_count (MockSetCursorPosition, Row, 1);
  will_return_count (MockSetCursorPosition, EFI_SUCCESS, 1);

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  KeyData1.Key.UnicodeChar = '1';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  will_return (SysInfoMgr, 6);
  will_return (SysInfoMgr, EFI_SUCCESS);

  KeyData2.Key.UnicodeChar = 'y';
  KeyData2.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData2);

  will_return (ResetCold, &JumpBuf);

  if (!SetJump (&JumpBuf)) {
    ConfAppEntry (NULL, NULL);
  }

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConfAppEntry of ConfApp when selecting 3.

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
ConfAppEntrySelect3 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_KEY_DATA              KeyData1;
  EFI_KEY_DATA              KeyData2;
  BASE_LIBRARY_JUMP_BUFFER  JumpBuf;

  will_return (MockSetWatchdogTimer, EFI_SUCCESS);

  expect_value (MockEnableCursor, Visible, FALSE);
  will_return (MockEnableCursor, EFI_SUCCESS);

  expect_value (MockLocateProtocol, Protocol, &gDfciSettingAccessProtocolGuid);
  will_return (MockLocateProtocol, NULL);

  expect_value (MockLocateProtocol, Protocol, &gDfciAuthenticationProtocolGuid);
  will_return (MockLocateProtocol, &MockAuthProtocol);

  will_return (MockAuthWithPW, 0xFEEDF00D);
  will_return (MockGetEnrolledIdentities, DFCI_IDENTITY_LOCAL);

  expect_any_count (MockSetCursorPosition, Column, 1);
  expect_any_count (MockSetCursorPosition, Row, 1);
  will_return_count (MockSetCursorPosition, EFI_SUCCESS, 1);

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  KeyData1.Key.UnicodeChar = '3';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  will_return (BootOptionMgr, 6);
  will_return (BootOptionMgr, EFI_SUCCESS);

  KeyData2.Key.UnicodeChar = 'y';
  KeyData2.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData2);

  will_return (ResetCold, &JumpBuf);

  if (!SetJump (&JumpBuf)) {
    ConfAppEntry (NULL, NULL);
  }

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConfAppEntry of ConfApp when selecting 4.

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
ConfAppEntrySelect4 (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_KEY_DATA              KeyData1;
  EFI_KEY_DATA              KeyData2;
  BASE_LIBRARY_JUMP_BUFFER  JumpBuf;

  will_return (MockSetWatchdogTimer, EFI_SUCCESS);

  expect_value (MockEnableCursor, Visible, FALSE);
  will_return (MockEnableCursor, EFI_SUCCESS);

  expect_value (MockLocateProtocol, Protocol, &gDfciSettingAccessProtocolGuid);
  will_return (MockLocateProtocol, NULL);

  expect_value (MockLocateProtocol, Protocol, &gDfciAuthenticationProtocolGuid);
  will_return (MockLocateProtocol, &MockAuthProtocol);

  will_return (MockAuthWithPW, 0xFEEDF00D);
  will_return (MockGetEnrolledIdentities, DFCI_IDENTITY_LOCAL);

  expect_any_count (MockSetCursorPosition, Column, 1);
  expect_any_count (MockSetCursorPosition, Row, 1);
  will_return_count (MockSetCursorPosition, EFI_SUCCESS, 1);

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  KeyData1.Key.UnicodeChar = '4';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  will_return (SetupConfMgr, 6);
  will_return (SetupConfMgr, EFI_SUCCESS);

  KeyData2.Key.UnicodeChar = 'y';
  KeyData2.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData2);

  will_return (ResetCold, &JumpBuf);

  if (!SetJump (&JumpBuf)) {
    ConfAppEntry (NULL, NULL);
  }

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConfAppEntry of ConfApp when selecting h.

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
ConfAppEntrySelectH (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_KEY_DATA              KeyData1;
  EFI_KEY_DATA              KeyData2;
  EFI_KEY_DATA              KeyData3;
  BASE_LIBRARY_JUMP_BUFFER  JumpBuf;

  will_return (MockSetWatchdogTimer, EFI_SUCCESS);

  expect_value (MockEnableCursor, Visible, FALSE);
  will_return (MockEnableCursor, EFI_SUCCESS);

  expect_value (MockLocateProtocol, Protocol, &gDfciSettingAccessProtocolGuid);
  will_return (MockLocateProtocol, NULL);

  expect_value (MockLocateProtocol, Protocol, &gDfciAuthenticationProtocolGuid);
  will_return (MockLocateProtocol, &MockAuthProtocol);

  will_return (MockAuthWithPW, 0xFEEDF00D);
  will_return (MockGetEnrolledIdentities, DFCI_IDENTITY_LOCAL);

  expect_any_count (MockSetCursorPosition, Column, 2);
  expect_any_count (MockSetCursorPosition, Row, 2);
  will_return_count (MockSetCursorPosition, EFI_SUCCESS, 2);

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  KeyData1.Key.UnicodeChar = 'h';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  will_return (MockClearScreen, EFI_SUCCESS);

  KeyData2.Key.UnicodeChar = CHAR_NULL;
  KeyData2.Key.ScanCode    = SCAN_ESC;
  will_return (MockReadKey, &KeyData2);

  KeyData3.Key.UnicodeChar = 'y';
  KeyData3.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData3);

  will_return (ResetCold, &JumpBuf);

  if (!SetJump (&JumpBuf)) {
    ConfAppEntry (NULL, NULL);
  }

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConfAppEntry of ConfApp when selecting ESC.

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
ConfAppEntrySelectEsc (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_KEY_DATA              KeyData1;
  EFI_KEY_DATA              KeyData2;
  BASE_LIBRARY_JUMP_BUFFER  JumpBuf;

  will_return (MockSetWatchdogTimer, EFI_SUCCESS);

  expect_value (MockEnableCursor, Visible, FALSE);
  will_return (MockEnableCursor, EFI_SUCCESS);

  expect_value (MockLocateProtocol, Protocol, &gDfciSettingAccessProtocolGuid);
  will_return (MockLocateProtocol, NULL);

  expect_value (MockLocateProtocol, Protocol, &gDfciAuthenticationProtocolGuid);
  will_return (MockLocateProtocol, &MockAuthProtocol);

  will_return (MockAuthWithPW, 0xFEEDF00D);
  will_return (MockGetEnrolledIdentities, DFCI_IDENTITY_LOCAL);

  expect_any_count (MockSetCursorPosition, Column, 1);
  expect_any_count (MockSetCursorPosition, Row, 1);
  will_return_count (MockSetCursorPosition, EFI_SUCCESS, 1);

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  KeyData1.Key.UnicodeChar = CHAR_NULL;
  KeyData1.Key.ScanCode    = SCAN_ESC;
  will_return (MockReadKey, &KeyData1);

  KeyData2.Key.UnicodeChar = 'y';
  KeyData2.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData2);

  will_return (ResetCold, &JumpBuf);

  if (!SetJump (&JumpBuf)) {
    ConfAppEntry (NULL, NULL);
  }

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConfAppEntry of ConfApp when selecting others.

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
ConfAppEntrySelectOther (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_KEY_DATA              KeyData1;
  EFI_KEY_DATA              KeyData2;
  EFI_KEY_DATA              KeyData3;
  BASE_LIBRARY_JUMP_BUFFER  JumpBuf;

  will_return (MockSetWatchdogTimer, EFI_SUCCESS);

  expect_value (MockEnableCursor, Visible, FALSE);
  will_return (MockEnableCursor, EFI_SUCCESS);

  expect_value (MockLocateProtocol, Protocol, &gDfciSettingAccessProtocolGuid);
  will_return (MockLocateProtocol, NULL);

  expect_value (MockLocateProtocol, Protocol, &gDfciAuthenticationProtocolGuid);
  will_return (MockLocateProtocol, &MockAuthProtocol);

  will_return (MockAuthWithPW, 0xFEEDF00D);
  will_return (MockGetEnrolledIdentities, DFCI_IDENTITY_LOCAL);

  expect_any_count (MockSetCursorPosition, Column, 1);
  expect_any_count (MockSetCursorPosition, Row, 1);
  will_return_count (MockSetCursorPosition, EFI_SUCCESS, 1);

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  KeyData1.Key.UnicodeChar = 'q';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  KeyData2.Key.UnicodeChar = CHAR_NULL;
  KeyData2.Key.ScanCode    = SCAN_ESC;
  will_return (MockReadKey, &KeyData2);

  KeyData3.Key.UnicodeChar = 'y';
  KeyData3.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData3);

  will_return (ResetCold, &JumpBuf);

  if (!SetJump (&JumpBuf)) {
    ConfAppEntry (NULL, NULL);
  }

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
  AddTestCase (MiscTests, "ConfApp Select 1 should go to System Info", "Select1", ConfAppEntrySelect1, NULL, NULL, NULL);
  // TODO: this is not supported yet
  // AddTestCase (MiscTests, "ConfApp Select 2 should go to Secure Boot", "Select2", ConfAppEntrySelect2, NULL, NULL, NULL);
  AddTestCase (MiscTests, "ConfApp Select 3 should go to Boot Options", "Select3", ConfAppEntrySelect3, NULL, NULL, NULL);
  AddTestCase (MiscTests, "ConfApp Select 4 should go to Setup Option", "Select4", ConfAppEntrySelect4, NULL, NULL, NULL);
  AddTestCase (MiscTests, "ConfApp Select h should reprint the options", "SelectH", ConfAppEntrySelectH, NULL, NULL, NULL);
  AddTestCase (MiscTests, "ConfApp Select ESC should confirm reboot", "SelectEsc", ConfAppEntrySelectEsc, NULL, NULL, NULL);
  AddTestCase (MiscTests, "ConfApp Select other should do nothing", "SelectOther", ConfAppEntrySelectOther, NULL, NULL, NULL);

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
