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
#include <Library/ConfigVariableListLib.h>

#include <Library/UnitTestLib.h>

#include <Good_Config_Data.h>
#include "ConfDataSettingProviderUnitTest.h"

#define UNIT_TEST_APP_NAME     "Conf Data Setting Provider Unit Tests"
#define UNIT_TEST_APP_VERSION  "1.0"

#define KNOWN_GOOD_TAG_0xF0   0xF0
#define KNOWN_GOOD_TAG_0x70   0x70
#define KNOWN_GOOD_TAG_0x280  0x280
#define KNOWN_GOOD_TAG_0x180  0x180
#define KNOWN_GOOD_TAG_0x200  0x200
#define KNOWN_GOOD_TAG_0x10   0x10
#define KNOWN_GOOD_TAG_0x80   0x80

#define KNOWN_GOOD_TAG_COUNT          7
#define SINGLE_CONF_DATA_ID_LEN       (sizeof (SINGLE_SETTING_PROVIDER_START) + sizeof (UINT32) * 2)
#define KNOWN_GOOD_RUNTIME_VAR_COUNT  7
#define KNOWN_GOOD_RUNTIME_VAR_ATTR   (EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_RUNTIME_ACCESS)

typedef struct {
  UINTN    Tag;
  UINT8    *Data;
  UINTN    DataSize;
} TAG_DATA;

typedef struct {
  CHAR16    Name[15];
  UINT8     *Data;
  UINTN     DataSize;
  UINTN     NameSize;
} RUNTIME_DATA;

typedef struct {
  CONFIG_VAR_LIST_ENTRY    *VarListPtr;
  UINTN                    VarListCnt;
} CONTEXT_DATA;

TAG_DATA  mKnownGoodTags[KNOWN_GOOD_TAG_COUNT] = {
  { KNOWN_GOOD_TAG_0xF0,  mGood_Tag_0xF0,  sizeof (mGood_Tag_0xF0)  },
  { KNOWN_GOOD_TAG_0x70,  mGood_Tag_0x70,  sizeof (mGood_Tag_0x70)  },
  { KNOWN_GOOD_TAG_0x280, mGood_Tag_0x280, sizeof (mGood_Tag_0x280) },
  { KNOWN_GOOD_TAG_0x180, mGood_Tag_0x180, sizeof (mGood_Tag_0x180) },
  { KNOWN_GOOD_TAG_0x200, mGood_Tag_0x200, sizeof (mGood_Tag_0x200) },
  { KNOWN_GOOD_TAG_0x10,  mGood_Tag_0x10,  sizeof (mGood_Tag_0x10)  },
  { KNOWN_GOOD_TAG_0x80,  mGood_Tag_0x80,  sizeof (mGood_Tag_0x80)  }
};

RUNTIME_DATA  mKnownGoodRuntimeVars[KNOWN_GOOD_RUNTIME_VAR_COUNT] = {
  { L"COMPLEX_KNOB1a", mGood_Runtime_Var_0, 0x09, 15 },
  { L"COMPLEX_KNOB1b", mGood_Runtime_Var_1, 0x09, 15 },
  { L"COMPLEX_KNOB2",  mGood_Runtime_Var_2, 0x16, 14 },
  { L"INTEGER_KNOB",   mGood_Runtime_Var_3, 0x04, 13 },
  { L"BOOLEAN_KNOB",   mGood_Runtime_Var_4, 0x01, 13 },
  { L"DOUBLE_KNOB",    mGood_Runtime_Var_5, 0x08, 12 },
  { L"FLOAT_KNOB",     mGood_Runtime_Var_6, 0x04, 11 },
};

/**
  Mocked version of DfciRegisterProvider

  @param  This                 Protocol instance pointer.
  @param  Provider             Provider pointer to register

  @retval EFI_SUCCESS          The provider registered.
  @retval ERROR                The provider could not be registered.

**/
EFI_STATUS
EFIAPI
MockDfciRegisterProvider (
  IN DFCI_SETTING_PROVIDER_SUPPORT_PROTOCOL  *This,
  IN DFCI_SETTING_PROVIDER                   *Provider
  )
{
  assert_non_null (This);
  assert_non_null (Provider);
  assert_non_null (Provider->Id);

  DEBUG ((DEBUG_INFO, "%a Registering ID %a\n", __FUNCTION__, Provider->Id));

  check_expected (Provider->Id);

  return EFI_SUCCESS;
}

// Mocked version of setting provider register protocol instance.
DFCI_SETTING_PROVIDER_SUPPORT_PROTOCOL  mMockDfciSetting = {
  .RegisterProvider = MockDfciRegisterProvider
};

/**
  Mocked version of EfiCreateProtocolNotifyEvent.

  @param  ProtocolGuid    Supplies GUID of the protocol upon whose installation the event is fired.
  @param  NotifyTpl       Supplies the task priority level of the event notifications.
  @param  NotifyFunction  Supplies the function to notify when the event is signaled.
  @param  NotifyContext   The optional context parameter to pass to NotifyFunction.
  @param  Registration    A pointer to a memory location to receive the registration value.
                          This value is passed to LocateHandle() to obtain new handles that
                          have been added that support the ProtocolGuid-specified protocol.

  @return The notification event that was created.

**/
EFI_EVENT
EFIAPI
EfiCreateProtocolNotifyEvent (
  IN  EFI_GUID          *ProtocolGuid,
  IN  EFI_TPL           NotifyTpl,
  IN  EFI_EVENT_NOTIFY  NotifyFunction,
  IN  VOID              *NotifyContext   OPTIONAL,
  OUT VOID              **Registration
  )
{
  assert_ptr_equal (ProtocolGuid, &gDfciSettingsProviderSupportProtocolGuid);
  assert_ptr_equal (NotifyFunction, SettingsProviderSupportProtocolNotify);

  return EFI_SUCCESS;
}

/**
  Mocked version of RegisterVarStateVariablePolicy.

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
  assert_memory_equal (Namespace, &gSetupConfigPolicyVariableGuid, sizeof (EFI_GUID));
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
  assert_memory_equal (VendorGuid, &gSetupConfigPolicyVariableGuid, sizeof (EFI_GUID));
  assert_non_null (DataSize);

  DEBUG ((DEBUG_INFO, "%a Name: %s, GUID: %g, Size: %x\n", __FUNCTION__, VariableName, VendorGuid, *DataSize));

  check_expected (VariableName);
  Size = (UINTN)mock ();
  if (Size > *DataSize) {
    *DataSize = Size;
    return EFI_BUFFER_TOO_SMALL;
  } else {
    *DataSize = Size;
    RetData   = (VOID *)mock ();
    CopyMem (Data, RetData, Size);
  }

  return (EFI_STATUS)mock ();
}

/**
  Mocked version of SetVariable.

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
  @retval EFI_SECURITY_VIOLATION The variable could not be written due to EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS being set,
                                 but the AuthInfo does NOT pass the validation check carried out by the firmware.

  @retval EFI_NOT_FOUND          The variable trying to be updated or deleted was not found.

**/
STATIC
EFI_STATUS
EFIAPI
MockSetVariable (
  IN  CHAR16    *VariableName,
  IN  EFI_GUID  *VendorGuid,
  IN  UINT32    Attributes,
  IN  UINTN     DataSize,
  IN  VOID      *Data
  )
{
  assert_non_null (VariableName);

  DEBUG ((DEBUG_INFO, "%a Name: %s\n", __FUNCTION__, VariableName));

  check_expected (VariableName);
  check_expected (VendorGuid);
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
  DEBUG ((DEBUG_INFO, "%a - %g\n", __FUNCTION__, Protocol));
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
    EFI_BOOT_SERVICES_SIGNATURE,         // Signature
    EFI_BOOT_SERVICES_REVISION,          // Revision
    sizeof (EFI_BOOT_SERVICES),          // HeaderSize
    0,                                   // CRC32
    0
  },
  .LocateProtocol = MockLocateProtocol,  // LocateProtocol
};

// System table, not used in this test.
EFI_SYSTEM_TABLE  MockSys;

UNIT_TEST_STATUS
EFIAPI
ConfSettingProviderPrerequisite (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CONTEXT_DATA  *Ctx;
  UINTN         Index;

  UT_ASSERT_NOT_NULL (Context);

  Ctx             = (CONTEXT_DATA *)Context;
  Ctx->VarListCnt = KNOWN_GOOD_TAG_COUNT + KNOWN_GOOD_RUNTIME_VAR_COUNT;
  Ctx->VarListPtr = AllocatePool (Ctx->VarListCnt * sizeof (CONFIG_VAR_LIST_ENTRY));
  UT_ASSERT_NOT_NULL (Ctx->VarListPtr);

  for (Index = 0; Index < KNOWN_GOOD_TAG_COUNT; Index++) {
    Ctx->VarListPtr[Index].Name = AllocatePool (SINGLE_CONF_DATA_ID_LEN * 2);
    UnicodeSPrintAsciiFormat (Ctx->VarListPtr[Index].Name, SINGLE_CONF_DATA_ID_LEN * 2, SINGLE_SETTING_PROVIDER_TEMPLATE, mKnownGoodTags[Index].Tag);
    Ctx->VarListPtr[Index].Attributes = CDATA_NV_VAR_ATTR;
    Ctx->VarListPtr[Index].DataSize   = (UINT32)(mKnownGoodTags[Index].DataSize);
    Ctx->VarListPtr[Index].Data       = AllocateCopyPool (mKnownGoodTags[Index].DataSize, mKnownGoodTags[Index].Data);
    CopyMem (&Ctx->VarListPtr[Index].Guid, &gSetupConfigPolicyVariableGuid, sizeof (EFI_GUID));
  }

  for (Index = 0; Index < KNOWN_GOOD_RUNTIME_VAR_COUNT; Index++) {
    Ctx->VarListPtr[Index + KNOWN_GOOD_TAG_COUNT].Name       = AllocateCopyPool (mKnownGoodRuntimeVars[Index].NameSize * sizeof (CHAR16), mKnownGoodRuntimeVars[Index].Name);
    Ctx->VarListPtr[Index + KNOWN_GOOD_TAG_COUNT].Attributes = KNOWN_GOOD_RUNTIME_VAR_ATTR;
    Ctx->VarListPtr[Index + KNOWN_GOOD_TAG_COUNT].DataSize   = (UINT32)(mKnownGoodRuntimeVars[Index].DataSize);
    Ctx->VarListPtr[Index + KNOWN_GOOD_TAG_COUNT].Data       = AllocateCopyPool (mKnownGoodRuntimeVars[Index].DataSize, mKnownGoodRuntimeVars[Index].Data);
    CopyMem (&Ctx->VarListPtr[Index + KNOWN_GOOD_TAG_COUNT].Guid, &gSetupConfigPolicyVariableGuid, sizeof (EFI_GUID));
  }

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SingleConfDataGetDefault of ConfDataSettingProvider with normal input.

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
SingleConfDataGetDefaultNormal (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS             Status;
  UINTN                  Size  = 0;
  VOID                   *Data = &Size;
  CHAR8                  ComparePtr[SINGLE_CONF_DATA_ID_LEN];
  DFCI_SETTING_PROVIDER  SettingsProvider;
  CONTEXT_DATA           *Ctx;
  RUNTIME_VAR_LIST_HDR   *VarListHdr;
  UINTN                  NeededSize;
  UINT32                 CRC32;

  AsciiSPrint (ComparePtr, SINGLE_CONF_DATA_ID_LEN, SINGLE_SETTING_PROVIDER_TEMPLATE, KNOWN_GOOD_TAG_0xF0);

  // Minimal initialization to tag this provider instance
  SettingsProvider.Id = ComparePtr;

  UT_ASSERT_NOT_NULL (Context);
  Ctx = (CONTEXT_DATA *)Context;

  expect_value (QuerySingleActiveConfigAsciiVarList, VarListName, SettingsProvider.Id);
  will_return (QuerySingleActiveConfigAsciiVarList, Ctx->VarListPtr);
  will_return (QuerySingleActiveConfigAsciiVarList, EFI_SUCCESS);

  Status = SingleConfDataGetDefault (&SettingsProvider, &Size, Data);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_BUFFER_TOO_SMALL);

  expect_value (QuerySingleActiveConfigAsciiVarList, VarListName, SettingsProvider.Id);
  will_return (QuerySingleActiveConfigAsciiVarList, Ctx->VarListPtr);
  will_return (QuerySingleActiveConfigAsciiVarList, EFI_SUCCESS);

  Data   = AllocatePool (Size);
  Status = SingleConfDataGetDefault (&SettingsProvider, &Size, Data);
  UT_ASSERT_NOT_EFI_ERROR (Status);

  VarListHdr = (RUNTIME_VAR_LIST_HDR *)Data;

  NeededSize = 0;
  UT_ASSERT_EQUAL (VarListHdr->NameSize, StrnLenS (Ctx->VarListPtr->Name, DFCI_MAX_ID_LEN) + 1);
  UT_ASSERT_EQUAL (VarListHdr->DataSize, sizeof (mGood_Tag_0xF0));
  NeededSize += sizeof (*VarListHdr);
  UT_ASSERT_MEM_EQUAL ((UINT8 *)Data + NeededSize, Ctx->VarListPtr->Name, VarListHdr->NameSize);
  NeededSize += VarListHdr->NameSize;
  UT_ASSERT_MEM_EQUAL ((UINT8 *)Data + NeededSize, &Ctx->VarListPtr->Guid, sizeof (Ctx->VarListPtr->Guid));
  NeededSize += sizeof (Ctx->VarListPtr->Guid);
  UT_ASSERT_MEM_EQUAL ((UINT8 *)Data + NeededSize, &Ctx->VarListPtr->Attributes, sizeof (Ctx->VarListPtr->Attributes));
  NeededSize += sizeof (Ctx->VarListPtr->Attributes);
  UT_ASSERT_MEM_EQUAL ((UINT8 *)Data + NeededSize, mGood_Tag_0xF0, sizeof (mGood_Tag_0xF0));
  NeededSize += sizeof (mGood_Tag_0xF0);

  CRC32 = CalculateCrc32 (Data, NeededSize);
  UT_ASSERT_EQUAL (*(UINT32 *)((UINT8 *)Data + NeededSize), CRC32);
  NeededSize += sizeof (CRC32);
  UT_ASSERT_EQUAL (Size, NeededSize);

  FreePool (Data);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SingleConfDataGetDefault of ConfDataSettingProvider with null provider.

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
SingleConfDataGetDefaultNull (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINTN       Size  = 0;
  VOID        *Data = &Size;
  // Minimal initialization to tag this provider instance
  DFCI_SETTING_PROVIDER  SettingsProvider = {
    .Id = DFCI_OEM_SETTING_ID__CONF
  };

  Status = SingleConfDataGetDefault (NULL, &Size, Data);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = SingleConfDataGetDefault (&SettingsProvider, NULL, Data);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SingleConfDataGetDefault of ConfDataSettingProvider with off target ID provider.

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
SingleConfDataGetDefaultNonTarget (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS             Status;
  UINTN                  Size  = 0;
  VOID                   *Data = &Size;
  DFCI_SETTING_PROVIDER  SettingsProvider;

  expect_any_count (QuerySingleActiveConfigAsciiVarList, VarListName, 2);

  will_return (QuerySingleActiveConfigAsciiVarList, NULL);
  will_return (QuerySingleActiveConfigAsciiVarList, EFI_INVALID_PARAMETER);
  SettingsProvider.Id = SINGLE_SETTING_PROVIDER_START;
  Status              = SingleConfDataGetDefault (&SettingsProvider, &Size, Data);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  will_return (QuerySingleActiveConfigAsciiVarList, NULL);
  will_return (QuerySingleActiveConfigAsciiVarList, EFI_INVALID_PARAMETER);
  SettingsProvider.Id = SINGLE_SETTING_PROVIDER_TEMPLATE;
  Status              = SingleConfDataGetDefault (&SettingsProvider, &Size, Data);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SingleConfDataGetDefault of ConfDataSettingProvider with invalid ID provider.

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
SingleConfDataGetDefaultInvalidId (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS             Status;
  UINTN                  Size  = 0;
  VOID                   *Data = &Size;
  CHAR8                  ComparePtr[SINGLE_CONF_DATA_ID_LEN];
  DFCI_SETTING_PROVIDER  SettingsProvider;

  AsciiSPrint (ComparePtr, SINGLE_CONF_DATA_ID_LEN, SINGLE_SETTING_PROVIDER_TEMPLATE, 0x40);

  // Minimal initialization to tag this provider instance
  SettingsProvider.Id = ComparePtr;

  expect_any (QuerySingleActiveConfigAsciiVarList, VarListName);
  will_return (QuerySingleActiveConfigAsciiVarList, NULL);
  will_return (QuerySingleActiveConfigAsciiVarList, EFI_NOT_FOUND);

  Status = SingleConfDataGetDefault (&SettingsProvider, &Size, Data);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_NOT_FOUND);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SingleConfDataGet of ConfDataSettingProvider with normal input.

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
SingleConfDataGetNormal (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS             Status;
  UINTN                  Size  = 0;
  VOID                   *Data = &Size;
  CHAR8                  ComparePtr[SINGLE_CONF_DATA_ID_LEN];
  CHAR16                 CompareUniPtr[SINGLE_CONF_DATA_ID_LEN];
  DFCI_SETTING_PROVIDER  SettingsProvider;
  CONTEXT_DATA           *Ctx;

  UT_ASSERT_NOT_NULL (Context);
  Ctx = (CONTEXT_DATA *)Context;

  AsciiSPrint (ComparePtr, sizeof (ComparePtr), SINGLE_SETTING_PROVIDER_TEMPLATE, KNOWN_GOOD_TAG_0xF0);
  UnicodeSPrintAsciiFormat (CompareUniPtr, sizeof (CompareUniPtr), SINGLE_SETTING_PROVIDER_TEMPLATE, KNOWN_GOOD_TAG_0xF0);

  // Minimal initialization to tag this provider instance
  SettingsProvider.Id = ComparePtr;

  expect_value (QuerySingleActiveConfigAsciiVarList, VarListName, SettingsProvider.Id);
  will_return (QuerySingleActiveConfigAsciiVarList, Ctx->VarListPtr);
  will_return (QuerySingleActiveConfigAsciiVarList, EFI_SUCCESS);

  expect_memory (MockGetVariable, VariableName, CompareUniPtr, sizeof (CompareUniPtr));
  will_return (MockGetVariable, sizeof (mGood_Tag_0xF0));

  Status = SingleConfDataGet (&SettingsProvider, &Size, Data);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_BUFFER_TOO_SMALL);

  expect_value (QuerySingleActiveConfigAsciiVarList, VarListName, SettingsProvider.Id);
  will_return (QuerySingleActiveConfigAsciiVarList, Ctx->VarListPtr);
  will_return (QuerySingleActiveConfigAsciiVarList, EFI_SUCCESS);

  expect_memory (MockGetVariable, VariableName, CompareUniPtr, sizeof (CompareUniPtr));
  will_return (MockGetVariable, sizeof (mGood_Tag_0xF0));

  expect_memory (MockGetVariable, VariableName, CompareUniPtr, sizeof (CompareUniPtr));
  will_return (MockGetVariable, sizeof (mGood_Tag_0xF0));
  will_return (MockGetVariable, mGood_Tag_0xF0);
  will_return (MockGetVariable, EFI_SUCCESS);

  Data   = AllocatePool (Size);
  Status = SingleConfDataGet (&SettingsProvider, &Size, Data);
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_MEM_EQUAL (Data, mGood_Tag_0xF0, sizeof (mGood_Tag_0xF0));

  FreePool (Data);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SingleConfDataGet of ConfDataSettingProvider with null provider.

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
SingleConfDataGetNull (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINTN       Size  = 0;
  VOID        *Data = &Size;
  // Minimal initialization to tag this provider instance
  DFCI_SETTING_PROVIDER  SettingsProvider = {
    .Id = DFCI_OEM_SETTING_ID__CONF
  };

  Status = SingleConfDataGet (NULL, &Size, Data);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = SingleConfDataGet (&SettingsProvider, NULL, Data);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SingleConfDataGet of ConfDataSettingProvider with off target ID provider.

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
SingleConfDataGetNonTarget (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS             Status;
  UINTN                  Size  = 0;
  VOID                   *Data = &Size;
  DFCI_SETTING_PROVIDER  SettingsProvider;

  expect_any_count (QuerySingleActiveConfigAsciiVarList, VarListName, 2);

  will_return (QuerySingleActiveConfigAsciiVarList, NULL);
  will_return (QuerySingleActiveConfigAsciiVarList, EFI_INVALID_PARAMETER);
  SettingsProvider.Id = SINGLE_SETTING_PROVIDER_START;
  Status              = SingleConfDataGet (&SettingsProvider, &Size, Data);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  will_return (QuerySingleActiveConfigAsciiVarList, NULL);
  will_return (QuerySingleActiveConfigAsciiVarList, EFI_INVALID_PARAMETER);
  SettingsProvider.Id = SINGLE_SETTING_PROVIDER_TEMPLATE;
  Status              = SingleConfDataGet (&SettingsProvider, &Size, Data);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SingleConfDataGet of ConfDataSettingProvider with invalid ID provider.

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
SingleConfDataGetInvalidId (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS             Status;
  UINTN                  Size  = 0;
  VOID                   *Data = &Size;
  CHAR8                  ComparePtr[SINGLE_CONF_DATA_ID_LEN];
  CHAR16                 CompareUniPtr[SINGLE_CONF_DATA_ID_LEN];
  DFCI_SETTING_PROVIDER  SettingsProvider;

  AsciiSPrint (ComparePtr, SINGLE_CONF_DATA_ID_LEN, SINGLE_SETTING_PROVIDER_TEMPLATE, 0x40);
  UnicodeSPrintAsciiFormat (CompareUniPtr, sizeof (CompareUniPtr), SINGLE_SETTING_PROVIDER_TEMPLATE, 0x40);

  // Minimal initialization to tag this provider instance
  SettingsProvider.Id = ComparePtr;

  expect_any (QuerySingleActiveConfigAsciiVarList, VarListName);
  will_return (QuerySingleActiveConfigAsciiVarList, NULL);
  will_return (QuerySingleActiveConfigAsciiVarList, EFI_NOT_FOUND);

  Status = SingleConfDataGet (&SettingsProvider, &Size, Data);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_NOT_FOUND);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SingleConfDataSetDefault of ConfDataSettingProvider with normal input.

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
SingleConfDataSetDefaultNormal (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS             Status;
  CHAR8                  ComparePtr[SINGLE_CONF_DATA_ID_LEN];
  CHAR16                 CompareUniPtr[SINGLE_CONF_DATA_ID_LEN];
  DFCI_SETTING_PROVIDER  SettingsProvider;
  CONTEXT_DATA           *Ctx;

  UT_ASSERT_NOT_NULL (Context);
  Ctx = (CONTEXT_DATA *)Context;

  AsciiSPrint (ComparePtr, sizeof (ComparePtr), SINGLE_SETTING_PROVIDER_TEMPLATE, KNOWN_GOOD_TAG_0xF0);
  UnicodeSPrintAsciiFormat (CompareUniPtr, sizeof (CompareUniPtr), SINGLE_SETTING_PROVIDER_TEMPLATE, KNOWN_GOOD_TAG_0xF0);

  // Minimal initialization to tag this provider instance
  SettingsProvider.Id = ComparePtr;

  expect_value (QuerySingleActiveConfigAsciiVarList, VarListName, SettingsProvider.Id);
  will_return (QuerySingleActiveConfigAsciiVarList, Ctx->VarListPtr);
  will_return (QuerySingleActiveConfigAsciiVarList, EFI_SUCCESS);

  expect_memory (MockSetVariable, VariableName, CompareUniPtr, sizeof (CompareUniPtr));
  expect_memory (MockSetVariable, VendorGuid, &gSetupConfigPolicyVariableGuid, sizeof (EFI_GUID));
  expect_value (MockSetVariable, DataSize, sizeof (mGood_Tag_0xF0));
  expect_memory (MockSetVariable, Data, mGood_Tag_0xF0, sizeof (mGood_Tag_0xF0));
  will_return (MockSetVariable, EFI_SUCCESS);

  Status = SingleConfDataSetDefault (&SettingsProvider);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SingleConfDataSetDefault of ConfDataSettingProvider with null provider.

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
SingleConfDataSetDefaultNull (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  // Minimal initialization to tag this provider instance
  DFCI_SETTING_PROVIDER  SettingsProvider = {
    .Id = NULL
  };

  Status = SingleConfDataSetDefault (NULL);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = SingleConfDataSetDefault (&SettingsProvider);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SingleConfDataSetDefault of ConfDataSettingProvider with off target ID provider.

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
SingleConfDataSetDefaultNonTarget (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS             Status;
  DFCI_SETTING_PROVIDER  SettingsProvider;

  expect_any (QuerySingleActiveConfigAsciiVarList, VarListName);
  will_return (QuerySingleActiveConfigAsciiVarList, NULL);
  will_return (QuerySingleActiveConfigAsciiVarList, EFI_NOT_FOUND);

  SettingsProvider.Id = SINGLE_SETTING_PROVIDER_START;
  Status              = SingleConfDataSetDefault (&SettingsProvider);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_NOT_FOUND);

  expect_any (QuerySingleActiveConfigAsciiVarList, VarListName);
  will_return (QuerySingleActiveConfigAsciiVarList, NULL);
  will_return (QuerySingleActiveConfigAsciiVarList, EFI_NOT_FOUND);

  SettingsProvider.Id = SINGLE_SETTING_PROVIDER_TEMPLATE;
  Status              = SingleConfDataSetDefault (&SettingsProvider);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_NOT_FOUND);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SingleConfDataSetDefault of ConfDataSettingProvider with invalid ID provider.

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
SingleConfDataSetDefaultInvalidId (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS             Status;
  CHAR8                  ComparePtr[SINGLE_CONF_DATA_ID_LEN];
  DFCI_SETTING_PROVIDER  SettingsProvider;

  AsciiSPrint (ComparePtr, SINGLE_CONF_DATA_ID_LEN, SINGLE_SETTING_PROVIDER_TEMPLATE, 0x40);

  // Minimal initialization to tag this provider instance
  SettingsProvider.Id = ComparePtr;

  expect_any (QuerySingleActiveConfigAsciiVarList, VarListName);
  will_return (QuerySingleActiveConfigAsciiVarList, NULL);
  will_return (QuerySingleActiveConfigAsciiVarList, EFI_NOT_FOUND);

  Status = SingleConfDataSetDefault (&SettingsProvider);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_NOT_FOUND);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SingleConfDataSet of ConfDataSettingProvider with normal input.

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
SingleConfDataSetNormal (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS             Status;
  CHAR8                  ComparePtr[SINGLE_CONF_DATA_ID_LEN];
  CHAR16                 CompareUniPtr[SINGLE_CONF_DATA_ID_LEN];
  DFCI_SETTING_PROVIDER  SettingsProvider;
  DFCI_SETTING_FLAGS     Flags = 0;
  CONTEXT_DATA           *Ctx;

  UT_ASSERT_NOT_NULL (Context);
  Ctx = (CONTEXT_DATA *)Context;

  AsciiSPrint (ComparePtr, sizeof (ComparePtr), SINGLE_SETTING_PROVIDER_TEMPLATE, KNOWN_GOOD_TAG_0xF0);
  UnicodeSPrintAsciiFormat (CompareUniPtr, sizeof (CompareUniPtr), SINGLE_SETTING_PROVIDER_TEMPLATE, KNOWN_GOOD_TAG_0xF0);

  // Minimal initialization to tag this provider instance
  SettingsProvider.Id = ComparePtr;

  expect_value (QuerySingleActiveConfigAsciiVarList, VarListName, SettingsProvider.Id);
  will_return (QuerySingleActiveConfigAsciiVarList, Ctx->VarListPtr);
  will_return (QuerySingleActiveConfigAsciiVarList, EFI_SUCCESS);

  expect_memory (MockSetVariable, VariableName, CompareUniPtr, sizeof (CompareUniPtr));
  expect_memory (MockSetVariable, VendorGuid, &(Ctx->VarListPtr->Guid), sizeof (EFI_GUID));
  expect_value (MockSetVariable, DataSize, sizeof (mGood_Tag_0xF0));
  expect_memory (MockSetVariable, Data, mGood_Tag_0xF0, sizeof (mGood_Tag_0xF0));
  will_return (MockSetVariable, EFI_SUCCESS);

  Status = SingleConfDataSet (&SettingsProvider, sizeof (mGood_Tag_0xF0_Var_List), mGood_Tag_0xF0_Var_List, &Flags);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_SUCCESS);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SingleConfDataSet of ConfDataSettingProvider with null provider.

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
SingleConfDataSetNull (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS          Status;
  UINTN               Size  = 1;
  VOID                *Data = &Size;
  DFCI_SETTING_FLAGS  Flags;
  // Minimal initialization to tag this provider instance
  DFCI_SETTING_PROVIDER  SettingsProvider = {
    .Id = DFCI_OEM_SETTING_ID__CONF
  };

  Status = SingleConfDataSet (NULL, Size, Data, &Flags);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = SingleConfDataSet (&SettingsProvider, 0, Data, &Flags);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = SingleConfDataSet (&SettingsProvider, 0, NULL, &Flags);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = SingleConfDataSet (&SettingsProvider, Size, Data, NULL);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SingleConfDataSet of ConfDataSettingProvider with off target ID provider.

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
SingleConfDataSetNonTarget (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS             Status;
  UINTN                  Size  = 1;
  VOID                   *Data = &Size;
  DFCI_SETTING_FLAGS     Flags;
  DFCI_SETTING_PROVIDER  SettingsProvider;

  SettingsProvider.Id = SINGLE_SETTING_PROVIDER_START;
  Status              = SingleConfDataSet (&SettingsProvider, Size, Data, &Flags);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  SettingsProvider.Id = SINGLE_SETTING_PROVIDER_TEMPLATE;
  Status              = SingleConfDataSet (&SettingsProvider, Size, Data, &Flags);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SingleConfDataSet of ConfDataSettingProvider with invalid ID provider.

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
SingleConfDataSetInvalidId (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS             Status;
  UINTN                  Size  = 1;
  VOID                   *Data = &Size;
  DFCI_SETTING_FLAGS     Flags;
  CHAR8                  ComparePtr[SINGLE_CONF_DATA_ID_LEN];
  DFCI_SETTING_PROVIDER  SettingsProvider;

  AsciiSPrint (ComparePtr, SINGLE_CONF_DATA_ID_LEN, SINGLE_SETTING_PROVIDER_TEMPLATE, 0x40);

  // Minimal initialization to tag this provider instance
  SettingsProvider.Id = ComparePtr;

  will_return (GetSectionFromAnyFv, mKnown_Good_Config_Data);
  will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Config_Data));

  will_return (GetSectionFromAnyFv, mKnown_Good_Config_Data);
  will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Config_Data));

  Status = SingleConfDataSet (&SettingsProvider, Size, Data, &Flags);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_NOT_FOUND);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for SettingsProviderSupportProtocolNotify of ConfDataSettingProvider.

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
SettingsProviderNotifyShouldComplete (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  CHAR8         *ComparePtr[KNOWN_GOOD_TAG_COUNT + KNOWN_GOOD_RUNTIME_VAR_COUNT];
  UINTN         Index = 0;
  CONTEXT_DATA  *Ctx;

  UT_ASSERT_NOT_NULL (Context);
  Ctx = (CONTEXT_DATA *)Context;
  UT_ASSERT_EQUAL (Ctx->VarListCnt, KNOWN_GOOD_TAG_COUNT + KNOWN_GOOD_RUNTIME_VAR_COUNT);

  expect_value (MockLocateProtocol, Protocol, &gDfciSettingsProviderSupportProtocolGuid);
  will_return (MockLocateProtocol, &mMockDfciSetting);

  expect_value (MockLocateProtocol, Protocol, &gEdkiiVariablePolicyProtocolGuid);
  will_return (MockLocateProtocol, &mMockDfciSetting);

  will_return (RetrieveActiveConfigVarList, Ctx->VarListPtr);
  will_return (RetrieveActiveConfigVarList, Ctx->VarListCnt);
  will_return (RetrieveActiveConfigVarList, EFI_SUCCESS);

  for (Index = 0; Index < KNOWN_GOOD_TAG_COUNT - 1; Index++) {
    ComparePtr[Index] = AllocatePool (SINGLE_CONF_DATA_ID_LEN);
    AsciiSPrint (ComparePtr[Index], SINGLE_CONF_DATA_ID_LEN, SINGLE_SETTING_PROVIDER_TEMPLATE, mKnownGoodTags[Index].Tag);
    expect_string (MockDfciRegisterProvider, Provider->Id, ComparePtr[Index]);
    expect_memory (MockGetVariable, VariableName, Ctx->VarListPtr[Index].Name, SINGLE_CONF_DATA_ID_LEN * sizeof (CHAR16));
    will_return (MockGetVariable, mKnownGoodTags[Index].DataSize);
    expect_memory (RegisterVarStateVariablePolicy, Name, Ctx->VarListPtr[Index].Name, SINGLE_CONF_DATA_ID_LEN * sizeof (CHAR16));
    expect_value (RegisterVarStateVariablePolicy, MinSize, mKnownGoodTags[Index].DataSize);
  }

  // Intentionally leave out the last entry to make sure it will be set if first seen
  ComparePtr[Index] = AllocatePool (SINGLE_CONF_DATA_ID_LEN);
  AsciiSPrint (ComparePtr[Index], SINGLE_CONF_DATA_ID_LEN, SINGLE_SETTING_PROVIDER_TEMPLATE, mKnownGoodTags[Index].Tag);
  expect_string (MockDfciRegisterProvider, Provider->Id, ComparePtr[Index]);
  expect_memory (MockGetVariable, VariableName, Ctx->VarListPtr[Index].Name, SINGLE_CONF_DATA_ID_LEN * sizeof (CHAR16));
  will_return (MockGetVariable, 0);
  will_return (MockGetVariable, NULL);
  will_return (MockGetVariable, EFI_NOT_FOUND);
  expect_memory (MockSetVariable, VariableName, Ctx->VarListPtr[Index].Name, SINGLE_CONF_DATA_ID_LEN * sizeof (CHAR16));
  expect_memory (MockSetVariable, VendorGuid, &gSetupConfigPolicyVariableGuid, sizeof (EFI_GUID));
  expect_value (MockSetVariable, DataSize, mKnownGoodTags[Index].DataSize);
  expect_memory (MockSetVariable, Data, mKnownGoodTags[Index].Data, mKnownGoodTags[Index].DataSize);
  will_return (MockSetVariable, EFI_SUCCESS);
  expect_memory (RegisterVarStateVariablePolicy, Name, Ctx->VarListPtr[Index].Name, SINGLE_CONF_DATA_ID_LEN * sizeof (CHAR16));
  expect_value (RegisterVarStateVariablePolicy, MinSize, mKnownGoodTags[Index].DataSize);

  for (Index = 0; Index < KNOWN_GOOD_RUNTIME_VAR_COUNT; Index++) {
    ComparePtr[Index + KNOWN_GOOD_TAG_COUNT] = AllocatePool (mKnownGoodRuntimeVars[Index].NameSize);
    AsciiSPrintUnicodeFormat (ComparePtr[Index + KNOWN_GOOD_TAG_COUNT], mKnownGoodRuntimeVars[Index].NameSize, L"%s", mKnownGoodRuntimeVars[Index].Name);
    expect_string (MockDfciRegisterProvider, Provider->Id, ComparePtr[Index + KNOWN_GOOD_TAG_COUNT]);
    expect_memory (MockGetVariable, VariableName, Ctx->VarListPtr[Index + KNOWN_GOOD_TAG_COUNT].Name, mKnownGoodRuntimeVars[Index].NameSize * sizeof (CHAR16));
    will_return (MockGetVariable, mKnownGoodRuntimeVars[Index].DataSize);
    expect_memory (RegisterVarStateVariablePolicy, Name, Ctx->VarListPtr[Index + KNOWN_GOOD_TAG_COUNT].Name, mKnownGoodRuntimeVars[Index].NameSize * sizeof (CHAR16));
    expect_value (RegisterVarStateVariablePolicy, MinSize, mKnownGoodRuntimeVars[Index].DataSize);
  }

  SettingsProviderSupportProtocolNotify (NULL, NULL);

  Index = 0;
  while (Index < KNOWN_GOOD_TAG_COUNT + KNOWN_GOOD_RUNTIME_VAR_COUNT) {
    FreePool (ComparePtr[Index]);
    Index++;
  }

  return UNIT_TEST_PASSED;
}

/**
  Initialize the unit test framework, suite, and unit tests for the
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
  UNIT_TEST_SUITE_HANDLE      ConfSettingTests;
  UNIT_TEST_SUITE_HANDLE      SingleSettingTests;
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
  Status = CreateUnitTestSuite (&ConfSettingTests, Framework, "ConfDataSettingProvider Conf Setting Tests", "ConfDataSettingProvider.Conf", NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in CreateUnitTestSuite for ConfSettingTests\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  Status = CreateUnitTestSuite (&SingleSettingTests, Framework, "ConfDataSettingProvider Single Setting Tests", "ConfDataSettingProvider.Single", NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in CreateUnitTestSuite for SingleSettingTests\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  Status = CreateUnitTestSuite (&MiscTests, Framework, "ConfDataSettingProvider Misc Tests", "ConfDataSettingProvider.Misc", NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in CreateUnitTestSuite for MiscTests\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  //
  // --------------Suite-----------Description--------------Name----------Function--------Pre---Post-------------------Context-----------
  //
  AddTestCase (MiscTests, "Protocol notify routine should succeed", "ProtocolNotify", SettingsProviderNotifyShouldComplete, ConfSettingProviderPrerequisite, NULL, &Context);

  // AddTestCase (SingleSettingTests, "Get Single ConfData Default should return full blob", "SingleGetFullDefaultNormal", SingleConfDataGetDefaultNormal, ConfSettingProviderPrerequisite, NULL, &Context);
  // AddTestCase (SingleSettingTests, "Get Single ConfData Default should fail with null provider", "SingleGetFullDefaultNull", SingleConfDataGetDefaultNull, NULL, NULL, NULL);
  // AddTestCase (SingleSettingTests, "Get Single ConfData Default should fail with provider of invalid ID", "SingleGetFullDefaultInvalidId", SingleConfDataGetDefaultInvalidId, NULL, NULL, NULL);
  // AddTestCase (SingleSettingTests, "Get Single ConfData Default should fail with provider of ID not related to config settings", "SingleGetDefaultNonTarget", SingleConfDataGetDefaultNonTarget, NULL, NULL, NULL);

  // AddTestCase (SingleSettingTests, "Get Single ConfData should return full blob", "SingleGetFullNormal", SingleConfDataGetNormal, ConfSettingProviderPrerequisite, NULL, &Context);
  // AddTestCase (SingleSettingTests, "Get Single ConfData should fail with null provider", "SingleGetFullNull", SingleConfDataGetNull, NULL, NULL, NULL);
  // AddTestCase (SingleSettingTests, "Get Single ConfData should fail with provider of invalid ID", "SingleGetFullInvalidId", SingleConfDataGetInvalidId, NULL, NULL, NULL);
  // AddTestCase (SingleSettingTests, "Get Single ConfData should fail with provider of ID not related to config settings", "SingleGetNonTarget", SingleConfDataGetNonTarget, NULL, NULL, NULL);

  // AddTestCase (SingleSettingTests, "Set Single ConfData Default should return full blob", "SingleSetDefaultNormal", SingleConfDataSetDefaultNormal, ConfSettingProviderPrerequisite, NULL, &Context);
  // AddTestCase (SingleSettingTests, "Set Single ConfData Default should fail with null provider", "SingleSetDefaultNull", SingleConfDataSetDefaultNull, NULL, NULL, NULL);
  // AddTestCase (SingleSettingTests, "Set Single ConfData Default should fail with provider of invalid ID", "SingleSetDefaultInvalidId", SingleConfDataSetDefaultInvalidId, NULL, NULL, NULL);
  // AddTestCase (SingleSettingTests, "Set Single ConfData Default should fail with provider of ID not related to config settings", "SingleSetDefaultNonTarget", SingleConfDataSetDefaultNonTarget, NULL, NULL, NULL);

  AddTestCase (SingleSettingTests, "Set Single ConfData should return full blob", "SingleSetNormal", SingleConfDataSetNormal, ConfSettingProviderPrerequisite, NULL, &Context);
  // AddTestCase (SingleSettingTests, "Set Single ConfData should fail with null provider", "SingleSetNull", SingleConfDataSetNull, NULL, NULL, NULL);
  // AddTestCase (SingleSettingTests, "Set Single ConfData should fail with provider of ID not related to config settings", "SingleSetNonTarget", SingleConfDataSetNonTarget, NULL, NULL, NULL);

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
