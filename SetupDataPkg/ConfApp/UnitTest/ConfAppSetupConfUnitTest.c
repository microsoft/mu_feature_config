/** @file
  Unit tests of the Setup Configuration page of ConfApp module

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
#include <Guid/VariableFormat.h>
#include <Guid/MuVarPolicyFoundationDxe.h>
#include <Protocol/VariablePolicy.h>
#include <Protocol/Policy.h>

#include <Library/BaseLib.h>
#include <Library/PrintLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/ConfigVariableListLib.h>

#include <Library/UnitTestLib.h>

#include <Good_Config_Data.h>
#include "ConfApp.h"

#define UNIT_TEST_APP_NAME     "Conf Application Setup Configuration Unit Tests"
#define UNIT_TEST_APP_VERSION  "1.0"

extern EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL  MockSimpleInput;
extern SetupConfState_t                   mSetupConfState;
extern POLICY_PROTOCOL                    *mPolicyProtocol;
extern volatile BOOLEAN                   gResetCalled;
typedef struct {
  UINTN    Tag;
  UINT8    *Data;
  UINTN    DataSize;
} TAG_DATA;

typedef struct {
  CONFIG_VAR_LIST_ENTRY    *VarListPtr;
  UINTN                    VarListCnt;
} CONTEXT_DATA;

EFI_STATUS
InspectDumpOutput (
  IN VOID   *Buffer,
  IN UINTN  BufferSize
  )
{
  assert_non_null (Buffer);
  DUMP_HEX (DEBUG_INFO, 0, Buffer, BufferSize, "");
  check_expected (BufferSize);
  check_expected (Buffer);

  return EFI_SUCCESS;
}

/**
*
*  Mocked version of SvdRequestXmlFromUSB.
*
*  @param[in]     FileName        What file to read.
*  @param[out]    JsonString      Where to store the Json String
*  @param[out]    JsonStringSize  Size of Json String
*
*  @retval   Status               Always returns success.
*
**/
EFI_STATUS
EFIAPI
SvdRequestXmlFromUSB (
  IN  CHAR16  *FileName,
  OUT CHAR8   **JsonString,
  OUT UINTN   *JsonStringSize
  )
{
  CHAR8  *Ret_Buff;

  check_expected (FileName);
  assert_non_null (JsonStringSize);

  *JsonStringSize = (UINTN)mock ();
  Ret_Buff        = (CHAR8 *)mock ();
  *JsonString     = AllocateCopyPool (*JsonStringSize, Ret_Buff);

  return EFI_SUCCESS;
}

/**
  State machine for system information page. It will display fundamental information, including
  UEFI version, system time, and configuration settings.

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
  State machine for secure boot page. It will react to user input from keystroke
  to set selected secure boot option or go back to previous page.

  @retval EFI_SUCCESS           This iteration of state machine proceeds successfully.
  @retval Others                Failed to wait for valid keystrokes or failed to set
                                platform key to variable service.
**/
EFI_STATUS
EFIAPI
SecureBootMgr (
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
 * Mock implementation of CpuDeadLoop to prevent actual deadlocks during testing.
 * This function immediately returns instead of causing an infinite loop,
 * allowing tests to run without hanging the system.
 *
 * @return None
 */
VOID
EFIAPI
MockCpuDeadLoop (
  VOID
  )
{
  return;
}

/**
  Mocked version of GetTime.

  @param[out]  Time             A pointer to storage to receive a snapshot of the current time.
  @param[out]  Capabilities     An optional pointer to a buffer to receive the real time clock
                                device's capabilities.

  @retval EFI_SUCCESS           The operation completed successfully.
  @retval EFI_INVALID_PARAMETER Time is NULL.
  @retval EFI_DEVICE_ERROR      The time could not be retrieved due to hardware error.

**/
EFI_STATUS
EFIAPI
MockGetTime (
  OUT  EFI_TIME               *Time,
  OUT  EFI_TIME_CAPABILITIES  *Capabilities OPTIONAL
  )
{
  EFI_TIME  DefaultPayloadTimestamp = {
    22,   // Year (2022)
    4,    // Month (April)
    29,   // Day (29)
  };

  assert_non_null (Time);
  CopyMem (Time, &DefaultPayloadTimestamp, sizeof (EFI_TIME));

  return EFI_SUCCESS;
}

///
/// Mock version of the UEFI Runtime Services Table
///
extern EFI_RUNTIME_SERVICES  MockRuntime;

/**
  Mocked version of MockWaitForEvent.

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

/**
  Mocked version of LocateProtocol.

  @param[in]  Protocol          Provides the protocol to search for.
  @param[in]  Registration      Optional registration key returned from
                                RegisterProtocolNotify().
  @param[out]  Interface        On return, a pointer to the first interface that matches Protocol and
                                Registration.

  @retval EFI_SUCCESS           A protocol instance matching Protocol was found and returned in
                                Interface.
  @retval EFI_NOT_FOUND         No protocol instances were found that match Protocol and
                                Registration.
  @retval EFI_INVALID_PARAMETER Interface is NULL.
                                Protocol is NULL.

**/
EFI_STATUS
EFIAPI
MockLocateProtocol (
  IN  EFI_GUID *Protocol,
  IN  VOID *Registration, OPTIONAL
  OUT VOID      **Interface
  )
{
  assert_non_null (Protocol);
  // Check that this is the right protocol being located
  check_expected_ptr (Protocol);

  // Set the protocol to one of our mock protocols
  *Interface = (VOID *)mock ();

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
  .WaitForEvent   = MockWaitForEvent,
  .LocateProtocol = MockLocateProtocol
};

/**
  Mocked version of GetPolicy.

  @param[in]      PolicyGuid        The GUID of the policy being retrieved.
  @param[out]     Attributes        The attributes of the stored policy.
  @param[out]     Policy            The buffer where the policy data is copied.
  @param[in,out]  PolicySize        The size of the stored policy data buffer.
                                    On output, contains the size of the stored policy.

  @retval   EFI_SUCCESS           The policy was retrieved.
  @retval   EFI_BUFFER_TOO_SMALL  The provided buffer size was too small.
  @retval   EFI_NOT_FOUND         The policy does not exist.
**/
EFI_STATUS
EFIAPI
MockGetPolicy (
  IN CONST EFI_GUID  *PolicyGuid,
  OUT UINT64         *Attributes OPTIONAL,
  OUT VOID           *Policy,
  IN OUT UINT16      *PolicySize
  )
{
  UINT16  Size;
  VOID    *Target;

  assert_non_null (PolicyGuid);
  DEBUG ((DEBUG_INFO, "%a - %g\n", __func__, PolicyGuid));
  // Check that this is the right protocol being located
  check_expected (PolicyGuid);

  assert_non_null (PolicySize);

  Size = (UINT16)mock ();

  if (Size > *PolicySize) {
    *PolicySize = Size;
    return EFI_BUFFER_TOO_SMALL;
  }

  if (Size == 0) {
    return EFI_NOT_FOUND;
  }

  assert_non_null (Policy);

  *PolicySize = Size;
  Target      = (VOID *)mock ();
  CopyMem (Policy, Target, Size);

  return EFI_SUCCESS;
}

///
/// Mock version of the UEFI Boot Services Table
///
POLICY_PROTOCOL  mMockedPolicy = {
  .GetPolicy = MockGetPolicy
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
SetupConfCleanup (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  // Leverage the exit state to clean up the global states.
  mSetupConfState = SetupConfExit;
  SetupConfMgr ();
  mSetupConfState = SetupConfInit;
  mPolicyProtocol = NULL;
}

/**
  Unit test for SetupConf page when selecting ESC.

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
ConfAppSetupConfInit (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  will_return (IsSystemInManufacturingMode, TRUE);
  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  expect_memory (MockLocateProtocol, Protocol, &gPolicyProtocolGuid, sizeof (EFI_GUID));
  will_return (MockLocateProtocol, &mMockedPolicy);

  expect_any (MockSetCursorPosition, Column);
  expect_any (MockSetCursorPosition, Row);
  will_return (MockSetCursorPosition, EFI_SUCCESS);

  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SetupConf page when selecting ESC.

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
ConfAppSetupConfSelectEsc (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS    Status;
  EFI_KEY_DATA  KeyData1;

  will_return (IsSystemInManufacturingMode, FALSE);
  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  expect_memory (MockLocateProtocol, Protocol, &gPolicyProtocolGuid, sizeof (EFI_GUID));
  will_return (MockLocateProtocol, &mMockedPolicy);

  expect_any (MockSetCursorPosition, Column);
  expect_any (MockSetCursorPosition, Row);
  will_return (MockSetCursorPosition, EFI_SUCCESS);

  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSetupConfState, SetupConfWait);

  mSimpleTextInEx = &MockSimpleInput;

  KeyData1.Key.UnicodeChar = CHAR_NULL;
  KeyData1.Key.ScanCode    = SCAN_ESC;
  will_return (MockReadKey, &KeyData1);

  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSetupConfState, SetupConfExit);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SetupConf page when selecting others.

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
ConfAppSetupConfSelectOther (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS    Status;
  EFI_KEY_DATA  KeyData1;

  will_return (IsSystemInManufacturingMode, FALSE);
  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  expect_memory (MockLocateProtocol, Protocol, &gPolicyProtocolGuid, sizeof (EFI_GUID));
  will_return (MockLocateProtocol, &mMockedPolicy);

  expect_any (MockSetCursorPosition, Column);
  expect_any (MockSetCursorPosition, Row);
  will_return (MockSetCursorPosition, EFI_SUCCESS);

  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSetupConfState, SetupConfWait);

  mSimpleTextInEx = &MockSimpleInput;

  KeyData1.Key.UnicodeChar = 'X';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSetupConfState, SetupConfWait);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SetupConf page when selecting configure from USB.

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
ConfAppSetupConfSelectUsb (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS    Status;
  EFI_KEY_DATA  KeyData1;

  gResetCalled = FALSE;

  will_return (IsSystemInManufacturingMode, TRUE);
  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  expect_memory (MockLocateProtocol, Protocol, &gPolicyProtocolGuid, sizeof (EFI_GUID));
  will_return (MockLocateProtocol, &mMockedPolicy);

  // Expect the prints twice
  expect_any (MockSetCursorPosition, Column);
  expect_any (MockSetCursorPosition, Row);
  will_return (MockSetCursorPosition, EFI_SUCCESS);

  // Initial run
  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSetupConfState, SetupConfWait);

  mSimpleTextInEx = &MockSimpleInput;

  KeyData1.Key.UnicodeChar = '1';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSetupConfState, SetupConfUpdateUsb);

  expect_memory (SvdRequestXmlFromUSB, FileName, PcdGetPtr (PcdConfigurationFileName), PcdGetSize (PcdConfigurationFileName));
  will_return (SvdRequestXmlFromUSB, sizeof (KNOWN_GOOD_VARLIST_XML) - 1);
  will_return (SvdRequestXmlFromUSB, KNOWN_GOOD_VARLIST_XML);

  will_return_always (MockSetVariable, EFI_SUCCESS);

  // first time through is delete
  expect_memory (MockSetVariable, VariableName, L"COMPLEX_KNOB1a", StrSize (L"COMPLEX_KNOB1a"));
  expect_memory (MockSetVariable, VendorGuid, &mKnown_Good_Xml_Guid, sizeof (EFI_GUID));
  expect_value (MockSetVariable, DataSize, 0x00);
  expect_value (MockSetVariable, Data, NULL);

  expect_memory (MockSetVariable, VariableName, L"COMPLEX_KNOB1a", StrSize (L"COMPLEX_KNOB1a"));
  expect_memory (MockSetVariable, VendorGuid, &mKnown_Good_Xml_Guid, sizeof (EFI_GUID));
  expect_value (MockSetVariable, DataSize, mKnown_Good_VarList_DataSizes[2]);
  expect_memory (MockSetVariable, Data, mKnown_Good_VarList_Entries[2], mKnown_Good_VarList_DataSizes[2]);

  // first time through is delete
  expect_memory (MockSetVariable, VariableName, L"INTEGER_KNOB", StrSize (L"INTEGER_KNOB"));
  expect_memory (MockSetVariable, VendorGuid, &mKnown_Good_Xml_Guid, sizeof (EFI_GUID));
  expect_value (MockSetVariable, DataSize, 0x00);
  expect_value (MockSetVariable, Data, NULL);

  expect_memory (MockSetVariable, VariableName, L"INTEGER_KNOB", StrSize (L"INTEGER_KNOB"));
  expect_memory (MockSetVariable, VendorGuid, &mKnown_Good_Xml_Guid, sizeof (EFI_GUID));
  expect_value (MockSetVariable, DataSize, mKnown_Good_VarList_DataSizes[5]);
  expect_memory (MockSetVariable, Data, mKnown_Good_VarList_Entries[5], mKnown_Good_VarList_DataSizes[5]);

  expect_value (ResetSystemWithSubtype, ResetType, EfiResetCold);
  expect_value (ResetSystemWithSubtype, ResetSubtype, &gConfAppResetGuid);

  SetupConfMgr ();
  UT_ASSERT_TRUE (gResetCalled); // Assert that reset was called

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SetupConf page when selecting configure from serial and passing in arbitrary SVD variables.

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
ConfAppSetupConfSelectSerialWithArbitrarySVD (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS    Status;
  EFI_KEY_DATA  KeyData1;
  UINTN         Index;
  CHAR8         *KnowGoodXml;

  will_return (IsSystemInManufacturingMode, TRUE);
  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  expect_memory (MockLocateProtocol, Protocol, &gPolicyProtocolGuid, sizeof (EFI_GUID));
  will_return (MockLocateProtocol, &mMockedPolicy);

  // Expect the prints twice
  expect_any (MockSetCursorPosition, Column);
  expect_any (MockSetCursorPosition, Row);
  will_return (MockSetCursorPosition, EFI_SUCCESS);

  // Initial run
  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSetupConfState, SetupConfWait);

  mSimpleTextInEx = &MockSimpleInput;

  KeyData1.Key.UnicodeChar = '2';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSetupConfState, SetupConfUpdateSerialHint);

  Index       = 0;
  KnowGoodXml = KNOWN_GOOD_VARLIST_XML;
  while (KnowGoodXml[Index] != 0) {
    KeyData1.Key.UnicodeChar = KnowGoodXml[Index];
    KeyData1.Key.ScanCode    = SCAN_NULL;
    will_return (MockReadKey, &KeyData1);
    Status = SetupConfMgr ();
    Index++;
  }

  KeyData1.Key.UnicodeChar = CHAR_CARRIAGE_RETURN;
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  will_return_always (MockSetVariable, EFI_SUCCESS);

  // first time through is delete
  expect_memory (MockSetVariable, VariableName, L"COMPLEX_KNOB1a", StrSize (L"COMPLEX_KNOB1a"));
  expect_memory (MockSetVariable, VendorGuid, &mKnown_Good_Xml_Guid, sizeof (EFI_GUID));
  expect_value (MockSetVariable, DataSize, 0x00);
  expect_value (MockSetVariable, Data, NULL);

  expect_memory (MockSetVariable, VariableName, L"COMPLEX_KNOB1a", StrSize (L"COMPLEX_KNOB1a"));
  expect_memory (MockSetVariable, VendorGuid, &mKnown_Good_Xml_Guid, sizeof (EFI_GUID));
  expect_value (MockSetVariable, DataSize, mKnown_Good_VarList_DataSizes[2]);
  expect_memory (MockSetVariable, Data, mKnown_Good_VarList_Entries[2], mKnown_Good_VarList_DataSizes[2]);

  // first time through is delete
  expect_memory (MockSetVariable, VariableName, L"INTEGER_KNOB", StrSize (L"INTEGER_KNOB"));
  expect_memory (MockSetVariable, VendorGuid, &mKnown_Good_Xml_Guid, sizeof (EFI_GUID));
  expect_value (MockSetVariable, DataSize, 0x00);
  expect_value (MockSetVariable, Data, NULL);

  expect_memory (MockSetVariable, VariableName, L"INTEGER_KNOB", StrSize (L"INTEGER_KNOB"));
  expect_memory (MockSetVariable, VendorGuid, &mKnown_Good_Xml_Guid, sizeof (EFI_GUID));
  expect_value (MockSetVariable, DataSize, mKnown_Good_VarList_DataSizes[5]);
  expect_memory (MockSetVariable, Data, mKnown_Good_VarList_Entries[5], mKnown_Good_VarList_DataSizes[5]);

  gResetCalled = FALSE;

  expect_value (ResetSystemWithSubtype, ResetType, EfiResetCold);
  expect_value (ResetSystemWithSubtype, ResetSubtype, &gConfAppResetGuid);

  SetupConfMgr ();
  UT_ASSERT_TRUE (gResetCalled); // Assert that reset was called

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SetupConf page when selecting configure from serial and return in the middle.

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
ConfAppSetupConfSelectSerialEsc (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS    Status;
  EFI_KEY_DATA  KeyData1;
  UINTN         Index;
  CHAR8         *KnowGoodXml;

  will_return (IsSystemInManufacturingMode, TRUE);
  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  expect_memory (MockLocateProtocol, Protocol, &gPolicyProtocolGuid, sizeof (EFI_GUID));
  will_return (MockLocateProtocol, &mMockedPolicy);

  // Expect the prints twice
  expect_any (MockSetCursorPosition, Column);
  expect_any (MockSetCursorPosition, Row);
  will_return (MockSetCursorPosition, EFI_SUCCESS);

  // Initial run
  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSetupConfState, SetupConfWait);

  mSimpleTextInEx = &MockSimpleInput;

  KeyData1.Key.UnicodeChar = '2';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSetupConfState, SetupConfUpdateSerialHint);

  Index       = 0;
  KnowGoodXml = KNOWN_GOOD_VARLIST_XML;
  while (KnowGoodXml[Index] != 0) {
    KeyData1.Key.UnicodeChar = KnowGoodXml[Index];
    KeyData1.Key.ScanCode    = SCAN_NULL;
    will_return (MockReadKey, &KeyData1);
    Status = SetupConfMgr ();
    UT_ASSERT_NOT_EFI_ERROR (Status);
    UT_ASSERT_EQUAL (mSetupConfState, SetupConfUpdateSerial);
    Index++;
  }

  KeyData1.Key.UnicodeChar = CHAR_NULL;
  KeyData1.Key.ScanCode    = SCAN_ESC;
  will_return (MockReadKey, &KeyData1);

  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSetupConfState, SetupConfExit);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SetupConf page when selecting dumping configure from serial.

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
ConfAppSetupConfDumpSerialMini (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS    Status;
  EFI_KEY_DATA  KeyData1;

  CONFIG_VAR_LIST_ENTRY  Entry;
  VOID                   *Buffer;
  UINTN                  BufferSize = 0;
  UINTN                  Offset;
  UINTN                  SizeLeft;
  UINT32                 TmpSize;

  will_return (IsSystemInManufacturingMode, FALSE);
  will_return_count (MockClearScreen, EFI_SUCCESS, 2);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  expect_memory (MockLocateProtocol, Protocol, &gPolicyProtocolGuid, sizeof (EFI_GUID));
  will_return (MockLocateProtocol, &mMockedPolicy);

  // Expect the prints twice
  expect_any_count (MockSetCursorPosition, Column, 2);
  expect_any_count (MockSetCursorPosition, Row, 2);
  will_return_count (MockSetCursorPosition, EFI_SUCCESS, 2);

  // Initial run
  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSetupConfState, SetupConfWait);

  mSimpleTextInEx = &MockSimpleInput;

  KeyData1.Key.UnicodeChar = '3';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSetupConfState, SetupConfDumpSerial);

  Status = GetVarListSize ((UINT32)StrSize (mKnown_Good_VarList_Names[2]), (UINT32)mKnown_Good_VarList_DataSizes[2], &TmpSize);
  UT_ASSERT_NOT_EFI_ERROR (Status);

  BufferSize += (UINTN)TmpSize;

  Status = GetVarListSize ((UINT32)StrSize (mKnown_Good_VarList_Names[5]), (UINT32)mKnown_Good_VarList_DataSizes[5], &TmpSize);
  UT_ASSERT_NOT_EFI_ERROR (Status);

  BufferSize += (UINTN)TmpSize;

  Buffer = AllocatePool (BufferSize);
  UT_ASSERT_NOT_NULL (Buffer);

  // Populate the first knob (COMPLEX_KNOB1a)
  Entry.Attributes = VARIABLE_ATTRIBUTE_BS_RT;
  Entry.Name       = mKnown_Good_VarList_Names[2];
  Entry.Guid       = mKnown_Good_Xml_Guid;
  Entry.Data       = mKnown_Good_VarList_Entries[2];
  Entry.DataSize   = mKnown_Good_VarList_DataSizes[2];
  Entry.Attributes = VARIABLE_ATTRIBUTE_BS_RT;

  Offset   = 0;
  SizeLeft = BufferSize - Offset;
  Status   = ConvertVariableEntryToVariableList (&Entry, Buffer, &SizeLeft);
  UT_ASSERT_NOT_EFI_ERROR (Status);
  Offset = SizeLeft;

  // Populate the second knob (INTEGER_KNOB)
  Entry.Attributes = VARIABLE_ATTRIBUTE_BS_RT;
  Entry.Name       = mKnown_Good_VarList_Names[5];
  Entry.Guid       = mKnown_Good_Xml_Guid;
  Entry.Data       = mKnown_Good_VarList_Entries[5];
  Entry.DataSize   = mKnown_Good_VarList_DataSizes[5];
  Entry.Attributes = VARIABLE_ATTRIBUTE_BS_RT;

  SizeLeft = BufferSize - Offset;
  ConvertVariableEntryToVariableList (&Entry, (UINT8 *)Buffer + Offset, &SizeLeft);
  UT_ASSERT_NOT_EFI_ERROR (Status);

  expect_memory_count (MockGetPolicy, PolicyGuid, &gZeroGuid, sizeof (EFI_GUID), 2);
  will_return_count (MockGetPolicy, BufferSize, 2);
  will_return (MockGetPolicy, Buffer);

  expect_value (InspectDumpOutput, BufferSize, sizeof (KNOWN_GOOD_VARLIST_SVD));
  expect_memory (InspectDumpOutput, Buffer, KNOWN_GOOD_VARLIST_SVD, sizeof (KNOWN_GOOD_VARLIST_SVD));

  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSetupConfState, SetupConfDumpComplete);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SetupConf page when selecting dumping configure from serial.

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
ConfAppSetupConfDumpSerial (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS    Status;
  EFI_KEY_DATA  KeyData1;

  will_return (IsSystemInManufacturingMode, FALSE);
  will_return_count (MockClearScreen, EFI_SUCCESS, 2);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  expect_memory (MockLocateProtocol, Protocol, &gPolicyProtocolGuid, sizeof (EFI_GUID));
  will_return (MockLocateProtocol, &mMockedPolicy);

  // Expect the prints twice
  expect_any_count (MockSetCursorPosition, Column, 2);
  expect_any_count (MockSetCursorPosition, Row, 2);
  will_return_count (MockSetCursorPosition, EFI_SUCCESS, 2);

  // Initial run
  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSetupConfState, SetupConfWait);

  mSimpleTextInEx = &MockSimpleInput;

  KeyData1.Key.UnicodeChar = '3';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSetupConfState, SetupConfDumpSerial);

  expect_memory_count (MockGetPolicy, PolicyGuid, &gZeroGuid, sizeof (EFI_GUID), 2);
  will_return_count (MockGetPolicy, sizeof (mKnown_Good_Generic_Profile), 2);
  will_return (MockGetPolicy, mKnown_Good_Generic_Profile);

  expect_any (InspectDumpOutput, BufferSize);
  expect_any (InspectDumpOutput, Buffer);

  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSetupConfState, SetupConfDumpComplete);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SetupConf page when selecting update configuration at non-mfg mode.

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
ConfAppSetupConfNonMfg (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS    Status;
  EFI_KEY_DATA  KeyData1;

  will_return (IsSystemInManufacturingMode, FALSE);
  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  expect_memory (MockLocateProtocol, Protocol, &gPolicyProtocolGuid, sizeof (EFI_GUID));
  will_return (MockLocateProtocol, &mMockedPolicy);

  // Expect the prints twice
  expect_any (MockSetCursorPosition, Column);
  expect_any (MockSetCursorPosition, Row);
  will_return (MockSetCursorPosition, EFI_SUCCESS);

  // Initial run
  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSetupConfState, SetupConfWait);

  mSimpleTextInEx = &MockSimpleInput;

  // Selecting 1 should fail
  KeyData1.Key.UnicodeChar = '1';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSetupConfState, SetupConfError);

  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSetupConfState, SetupConfWait);

  // Selecting 2 should fail
  KeyData1.Key.UnicodeChar = '2';
  KeyData1.Key.ScanCode    = SCAN_NULL;
  will_return (MockReadKey, &KeyData1);

  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSetupConfState, SetupConfError);

  Status = SetupConfMgr ();
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (mSetupConfState, SetupConfWait);

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

  MockRuntime.GetTime = MockGetTime;

  //
  // --------------Suite-----------Description--------------Name----------Function--------Pre---Post-------------------Context-----------
  //
  AddTestCase (MiscTests, "Setup Configuration page should initialize properly", "NormalInit", ConfAppSetupConfInit, NULL, SetupConfCleanup, NULL);
  AddTestCase (MiscTests, "Setup Configuration page select Esc should go to previous menu", "SelectEsc", ConfAppSetupConfSelectEsc, NULL, SetupConfCleanup, NULL);
  AddTestCase (MiscTests, "Setup Configuration page select others should do nothing", "SelectOther", ConfAppSetupConfSelectOther, NULL, SetupConfCleanup, NULL);
  AddTestCase (MiscTests, "Setup Configuration page should setup configuration from USB", "SelectUsb", ConfAppSetupConfSelectUsb, NULL, SetupConfCleanup, NULL);
  AddTestCase (MiscTests, "Setup Configuration page should setup configuration from serial", "SelectSerialWithArbitrarySVD", ConfAppSetupConfSelectSerialWithArbitrarySVD, NULL, SetupConfCleanup, NULL);
  AddTestCase (MiscTests, "Setup Configuration page should return with ESC key during serial transport", "SelectSerial", ConfAppSetupConfSelectSerialEsc, NULL, SetupConfCleanup, NULL);
  AddTestCase (MiscTests, "Setup Configuration page should dump 2 configurations from serial", "ConfDumpMini", ConfAppSetupConfDumpSerialMini, NULL, SetupConfCleanup, NULL);
  AddTestCase (MiscTests, "Setup Configuration page should dump all configurations from serial", "ConfDump", ConfAppSetupConfDumpSerial, NULL, SetupConfCleanup, NULL);
  AddTestCase (MiscTests, "Setup Configuration page should ignore updating configurations when in non-mfg mode", "ConfNonMfg", ConfAppSetupConfNonMfg, NULL, SetupConfCleanup, NULL);

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
