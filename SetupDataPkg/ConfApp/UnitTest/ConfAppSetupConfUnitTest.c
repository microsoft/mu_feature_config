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

typedef struct {
  UINTN    Tag;
  UINT8    *Data;
  UINTN    DataSize;
} TAG_DATA;

typedef struct {
  CONFIG_VAR_LIST_ENTRY    *VarListPtr;
  UINTN                    VarListCnt;
} CONTEXT_DATA;

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

UNIT_TEST_STATUS
EFIAPI
SetupConfPrerequisite (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CONTEXT_DATA  *Ctx;
  UINTN         Index;

  UT_ASSERT_NOT_NULL (Context);

  Ctx             = (CONTEXT_DATA *)Context;
  Ctx->VarListCnt = KNOWN_GOOD_TAG_COUNT;
  Ctx->VarListPtr = AllocatePool (Ctx->VarListCnt * sizeof (CONFIG_VAR_LIST_ENTRY));
  UT_ASSERT_NOT_NULL (Ctx->VarListPtr);

  for (Index = 0; Index < KNOWN_GOOD_TAG_COUNT; Index++) {
    Ctx->VarListPtr[Index].Name       = mKnown_Good_VarList_Names[Index];
    Ctx->VarListPtr[Index].Attributes = VARIABLE_ATTRIBUTE_BS_RT;
    Ctx->VarListPtr[Index].DataSize   = mKnown_Good_VarList_DataSizes[Index];
    Ctx->VarListPtr[Index].Data       = AllocateCopyPool (Ctx->VarListPtr[Index].DataSize, mKnown_Good_VarList_Entries[Index]);
    CopyMem (&Ctx->VarListPtr[Index].Guid, &gEfiCallerIdGuid, sizeof (EFI_GUID));
  }

  return UNIT_TEST_PASSED;
}

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

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  expect_memory (MockGetVariable, VariableName, READY_TO_BOOT_INDICATOR_VAR_NAME, sizeof (READY_TO_BOOT_INDICATOR_VAR_NAME));
  expect_memory (MockGetVariable, VendorGuid, &gMuVarPolicyDxePhaseGuid, sizeof (mKnown_Good_Xml_Guid_LE));

  will_return (MockGetVariable, 0);
  will_return (MockGetVariable, NULL);
  will_return (MockGetVariable, EFI_NOT_FOUND);

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
  EFI_STATUS       Status;
  EFI_KEY_DATA     KeyData1;
  POLICY_LOCK_VAR  LockVar = PHASE_INDICATOR_SET;

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  expect_memory (MockGetVariable, VariableName, READY_TO_BOOT_INDICATOR_VAR_NAME, sizeof (READY_TO_BOOT_INDICATOR_VAR_NAME));
  expect_memory (MockGetVariable, VendorGuid, &gMuVarPolicyDxePhaseGuid, sizeof (mKnown_Good_Xml_Guid_LE));

  will_return (MockGetVariable, sizeof (LockVar));
  will_return (MockGetVariable, &LockVar);
  will_return (MockGetVariable, EFI_SUCCESS);

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
  EFI_STATUS       Status;
  EFI_KEY_DATA     KeyData1;
  POLICY_LOCK_VAR  LockVar = PHASE_INDICATOR_SET;

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  expect_memory (MockGetVariable, VariableName, READY_TO_BOOT_INDICATOR_VAR_NAME, sizeof (READY_TO_BOOT_INDICATOR_VAR_NAME));
  expect_memory (MockGetVariable, VendorGuid, &gMuVarPolicyDxePhaseGuid, sizeof (mKnown_Good_Xml_Guid_LE));

  will_return (MockGetVariable, sizeof (LockVar));
  will_return (MockGetVariable, &LockVar);
  will_return (MockGetVariable, EFI_SUCCESS);

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
  EFI_STATUS                Status;
  EFI_KEY_DATA              KeyData1;
  BASE_LIBRARY_JUMP_BUFFER  JumpBuf;

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  expect_memory (MockGetVariable, VariableName, READY_TO_BOOT_INDICATOR_VAR_NAME, sizeof (READY_TO_BOOT_INDICATOR_VAR_NAME));
  expect_memory (MockGetVariable, VendorGuid, &gMuVarPolicyDxePhaseGuid, sizeof (mKnown_Good_Xml_Guid_LE));

  will_return (MockGetVariable, 0);
  will_return (MockGetVariable, NULL);
  will_return (MockGetVariable, EFI_NOT_FOUND);

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
  expect_memory (MockSetVariable, VendorGuid, &mKnown_Good_Xml_Guid_LE, sizeof (mKnown_Good_Xml_Guid_LE));
  expect_value (MockSetVariable, DataSize, 0x00);
  expect_value (MockSetVariable, Data, NULL);

  expect_memory (MockSetVariable, VariableName, L"COMPLEX_KNOB1a", StrSize (L"COMPLEX_KNOB1a"));
  expect_memory (MockSetVariable, VendorGuid, &mKnown_Good_Xml_Guid_LE, sizeof (mKnown_Good_Xml_Guid_LE));
  expect_value (MockSetVariable, DataSize, 0x09);
  expect_memory (MockSetVariable, Data, mKnown_Good_Xml_VarList_Entries[0], 0x09);

  // first time through is delete
  expect_memory (MockSetVariable, VariableName, L"INTEGER_KNOB", StrSize (L"INTEGER_KNOB"));
  expect_memory (MockSetVariable, VendorGuid, &mKnown_Good_Xml_Guid_LE, sizeof (mKnown_Good_Xml_Guid_LE));
  expect_value (MockSetVariable, DataSize, 0x00);
  expect_value (MockSetVariable, Data, NULL);

  expect_memory (MockSetVariable, VariableName, L"INTEGER_KNOB", StrSize (L"INTEGER_KNOB"));
  expect_memory (MockSetVariable, VendorGuid, &mKnown_Good_Xml_Guid_LE, sizeof (mKnown_Good_Xml_Guid_LE));
  expect_value (MockSetVariable, DataSize, 0x04);
  expect_memory (MockSetVariable, Data, mKnown_Good_Xml_VarList_Entries[1], 0x04);

  will_return (ResetCold, &JumpBuf);

  if (!SetJump (&JumpBuf)) {
    SetupConfMgr ();
  }

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
  EFI_STATUS                Status;
  EFI_KEY_DATA              KeyData1;
  UINTN                     Index;
  CHAR8                     *KnowGoodXml;
  BASE_LIBRARY_JUMP_BUFFER  JumpBuf;

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  expect_memory (MockGetVariable, VariableName, READY_TO_BOOT_INDICATOR_VAR_NAME, sizeof (READY_TO_BOOT_INDICATOR_VAR_NAME));
  expect_memory (MockGetVariable, VendorGuid, &gMuVarPolicyDxePhaseGuid, sizeof (mKnown_Good_Xml_Guid_LE));

  will_return (MockGetVariable, 0);
  will_return (MockGetVariable, NULL);
  will_return (MockGetVariable, EFI_NOT_FOUND);

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
  expect_memory (MockSetVariable, VendorGuid, &mKnown_Good_Xml_Guid_LE, sizeof (mKnown_Good_Xml_Guid_LE));
  expect_value (MockSetVariable, DataSize, 0x00);
  expect_value (MockSetVariable, Data, NULL);

  expect_memory (MockSetVariable, VariableName, L"COMPLEX_KNOB1a", StrSize (L"COMPLEX_KNOB1a"));
  expect_memory (MockSetVariable, VendorGuid, &mKnown_Good_Xml_Guid_LE, sizeof (mKnown_Good_Xml_Guid_LE));
  expect_value (MockSetVariable, DataSize, 0x09);
  expect_memory (MockSetVariable, Data, mKnown_Good_Xml_VarList_Entries[0], 0x09);

  // first time through is delete
  expect_memory (MockSetVariable, VariableName, L"INTEGER_KNOB", StrSize (L"INTEGER_KNOB"));
  expect_memory (MockSetVariable, VendorGuid, &mKnown_Good_Xml_Guid_LE, sizeof (mKnown_Good_Xml_Guid_LE));
  expect_value (MockSetVariable, DataSize, 0x00);
  expect_value (MockSetVariable, Data, NULL);

  expect_memory (MockSetVariable, VariableName, L"INTEGER_KNOB", StrSize (L"INTEGER_KNOB"));
  expect_memory (MockSetVariable, VendorGuid, &mKnown_Good_Xml_Guid_LE, sizeof (mKnown_Good_Xml_Guid_LE));
  expect_value (MockSetVariable, DataSize, 0x04);
  expect_memory (MockSetVariable, Data, mKnown_Good_Xml_VarList_Entries[1], 0x04);

  will_return (ResetCold, &JumpBuf);

  if (!SetJump (&JumpBuf)) {
    SetupConfMgr ();
  }

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

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  expect_memory (MockGetVariable, VariableName, READY_TO_BOOT_INDICATOR_VAR_NAME, sizeof (READY_TO_BOOT_INDICATOR_VAR_NAME));
  expect_memory (MockGetVariable, VendorGuid, &gMuVarPolicyDxePhaseGuid, sizeof (mKnown_Good_Xml_Guid_LE));

  will_return (MockGetVariable, 0);
  will_return (MockGetVariable, NULL);
  will_return (MockGetVariable, EFI_NOT_FOUND);

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
ConfAppSetupConfDumpSerial (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS       Status;
  EFI_KEY_DATA     KeyData1;
  CHAR16           *CompareUnicodePtr[KNOWN_GOOD_TAG_COUNT];
  UINTN            Index;
  UINTN            ReturnUnicodeSize;
  POLICY_LOCK_VAR  LockVar = PHASE_INDICATOR_SET;
  CONTEXT_DATA     *Ctx;

  UT_ASSERT_NOT_NULL (Context);
  Ctx = (CONTEXT_DATA *)Context;

  will_return_count (MockClearScreen, EFI_SUCCESS, 2);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  expect_memory (MockGetVariable, VariableName, READY_TO_BOOT_INDICATOR_VAR_NAME, sizeof (READY_TO_BOOT_INDICATOR_VAR_NAME));
  expect_memory (MockGetVariable, VendorGuid, &gMuVarPolicyDxePhaseGuid, sizeof (mKnown_Good_Xml_Guid_LE));

  will_return (MockGetVariable, sizeof (LockVar));
  will_return (MockGetVariable, &LockVar);
  will_return (MockGetVariable, EFI_SUCCESS);

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

  // This is to mimic config setting variables
  for (Index = 0; Index < Ctx->VarListCnt; Index++) {
    CompareUnicodePtr[Index] = Ctx->VarListPtr->Name;
    ReturnUnicodeSize        = StrnLenS (CompareUnicodePtr[Index], KNOWN_GOOD_TAG_NAME_LEN);

    // Only the first iteration needs to extend the var name buffer
    if (Index == 0) {
      will_return (MockGetNextVariableName, (ReturnUnicodeSize + 1) * sizeof (CHAR16));
    }

    will_return (MockGetNextVariableName, (ReturnUnicodeSize + 1) * sizeof (CHAR16));
    will_return (MockGetNextVariableName, CompareUnicodePtr[Index]);
    will_return (MockGetNextVariableName, &gEfiCallerIdGuid);

    // Respond to get var size query
    expect_memory (MockGetVariable, VariableName, CompareUnicodePtr[Index], ReturnUnicodeSize);
    expect_memory (MockGetVariable, VendorGuid, &Ctx->VarListPtr->Guid, sizeof (EFI_GUID));

    will_return (MockGetVariable, Ctx->VarListPtr->DataSize);

    // Get to the variable details
    expect_memory (MockGetVariable, VariableName, CompareUnicodePtr[Index], ReturnUnicodeSize);
    expect_memory (MockGetVariable, VendorGuid, &Ctx->VarListPtr->Guid, sizeof (EFI_GUID));

    will_return (MockGetVariable, Ctx->VarListPtr->DataSize);
    will_return (MockGetVariable, Ctx->VarListPtr->Data);
    will_return (MockGetVariable, EFI_SUCCESS);
  }

  // End the queries with a EFI_NOT_FOUND
  will_return (MockGetNextVariableName, 0);
  will_return (MockGetNextVariableName, EFI_NOT_FOUND);

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
  EFI_STATUS       Status;
  EFI_KEY_DATA     KeyData1;
  POLICY_LOCK_VAR  LockVar = PHASE_INDICATOR_SET;

  will_return (MockClearScreen, EFI_SUCCESS);
  will_return_always (MockSetAttribute, EFI_SUCCESS);

  expect_memory (MockGetVariable, VariableName, READY_TO_BOOT_INDICATOR_VAR_NAME, sizeof (READY_TO_BOOT_INDICATOR_VAR_NAME));
  expect_memory (MockGetVariable, VendorGuid, &gMuVarPolicyDxePhaseGuid, sizeof (mKnown_Good_Xml_Guid_LE));

  will_return (MockGetVariable, sizeof (LockVar));
  will_return (MockGetVariable, &LockVar);
  will_return (MockGetVariable, EFI_SUCCESS);

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
  CONTEXT_DATA                Context;

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
  AddTestCase (MiscTests, "Setup Configuration page should dump all configurations from serial", "ConfDump", ConfAppSetupConfDumpSerial, SetupConfPrerequisite, SetupConfCleanup, &Context);
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
