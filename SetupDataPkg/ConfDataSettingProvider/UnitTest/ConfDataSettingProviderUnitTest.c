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

  assert_memory_equal (NameGuid, PcdGetPtr (PcdConfigPolicyVariableGuid), sizeof (EFI_GUID));
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
  assert_memory_equal (VendorGuid, PcdGetPtr (PcdConfigPolicyVariableGuid), sizeof (EFI_GUID));
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

/**
  Unit test for ConfDataGetDefault of ConfDataSettingProvider with normal input.

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
ConfDataGetDefaultNormal (
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

  will_return (GetSectionFromAnyFv, mKnown_Good_Config_Data);
  will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Config_Data));

  will_return (GetSectionFromAnyFv, mKnown_Good_Config_Data);
  will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Config_Data));

  Status = ConfDataGetDefault (&SettingsProvider, &Size, Data);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_BUFFER_TOO_SMALL);
  UT_ASSERT_EQUAL (Size, sizeof (mKnown_Good_Config_Data));

  Data   = AllocatePool (Size);
  Status = ConfDataGetDefault (&SettingsProvider, &Size, Data);
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_MEM_EQUAL (Data, mKnown_Good_Config_Data, sizeof (mKnown_Good_Config_Data));

  FreePool (Data);
  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConfDataGetDefault of ConfDataSettingProvider with null provider.

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
ConfDataGetDefaultNull (
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

  Status = ConfDataGetDefault (NULL, &Size, Data);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = ConfDataGetDefault (&SettingsProvider, NULL, Data);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConfDataGetDefault of ConfDataSettingProvider with mismatched provider.

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
ConfDataGetDefaultMismatch (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINTN       Size  = 0;
  VOID        *Data = &Size;
  // Minimal initialization to tag this provider instance
  DFCI_SETTING_PROVIDER  SettingsProvider = {
    .Id = SINGLE_SETTING_PROVIDER_START
  };

  Status = ConfDataGetDefault (&SettingsProvider, &Size, Data);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_UNSUPPORTED);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConfDataGet of ConfDataSettingProvider with normal input.

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
ConfDataGetNormal (
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

  will_return (GetSectionFromAnyFv, mKnown_Good_Config_Data);
  will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Config_Data));

  will_return (GetSectionFromAnyFv, mKnown_Good_Config_Data);
  will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Config_Data));

  Status = ConfDataGet (&SettingsProvider, &Size, Data);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_BUFFER_TOO_SMALL);
  UT_ASSERT_EQUAL (Size, sizeof (mKnown_Good_Config_Data));

  Data   = AllocatePool (Size);
  Status = ConfDataGet (&SettingsProvider, &Size, Data);
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_MEM_EQUAL (Data, mKnown_Good_Config_Data, sizeof (mKnown_Good_Config_Data));

  FreePool (Data);
  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConfDataGet of ConfDataSettingProvider with null provider.

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
ConfDataGetNull (
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

  Status = ConfDataGet (NULL, &Size, Data);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = ConfDataGet (&SettingsProvider, NULL, Data);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConfDataGet of ConfDataSettingProvider with mismatched provider.

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
ConfDataGetMismatch (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINTN       Size  = 0;
  VOID        *Data = &Size;
  // Minimal initialization to tag this provider instance
  DFCI_SETTING_PROVIDER  SettingsProvider = {
    .Id = SINGLE_SETTING_PROVIDER_START
  };

  Status = ConfDataGet (&SettingsProvider, &Size, Data);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_UNSUPPORTED);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConfDataSetDefault of ConfDataSettingProvider with normal input.

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
ConfDataSetDefaultNormal (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  CHAR16      *CompareUniPtr[KNOWN_GOOD_TAG_COUNT];
  UINTN       UniStrSize = SINGLE_CONF_DATA_ID_LEN * sizeof (CHAR16);
  UINTN       Index      = 0;
  // Minimal initialization to tag this provider instance
  DFCI_SETTING_PROVIDER  SettingsProvider = {
    .Id = DFCI_OEM_SETTING_ID__CONF
  };

  will_return (GetSectionFromAnyFv, mKnown_Good_Config_Data);
  will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Config_Data));

  will_return (GetSectionFromAnyFv, mKnown_Good_Config_Data);
  will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Config_Data));

  will_return_always (MockSetVariable, EFI_SUCCESS);

  for (Index = 0; Index < KNOWN_GOOD_TAG_COUNT; Index++) {
    CompareUniPtr[Index] = AllocatePool (UniStrSize);
    UnicodeSPrintAsciiFormat (CompareUniPtr[Index], UniStrSize, SINGLE_SETTING_PROVIDER_TEMPLATE, mKnownGoodTags[Index].Tag);
    expect_memory (MockSetVariable, VariableName, CompareUniPtr[Index], UniStrSize);
    expect_value (MockSetVariable, DataSize, mKnownGoodTags[Index].DataSize);
    expect_memory (MockSetVariable, Data, mKnownGoodTags[Index].Data, mKnownGoodTags[Index].DataSize);
  }

  Status = ConfDataSetDefault (&SettingsProvider);
  UT_ASSERT_NOT_EFI_ERROR (Status);

  Index = 0;
  while (Index < KNOWN_GOOD_TAG_COUNT) {
    FreePool (CompareUniPtr[Index]);
    Index++;
  }

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConfDataSetDefault of ConfDataSettingProvider with null provider.

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
ConfDataSetDefaultNull (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  Status = ConfDataSetDefault (NULL);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConfDataSetDefault of ConfDataSettingProvider with mismatched provider.

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
ConfDataSetDefaultMismatch (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  // Minimal initialization to tag this provider instance
  DFCI_SETTING_PROVIDER  SettingsProvider = {
    .Id = SINGLE_SETTING_PROVIDER_START
  };

  Status = ConfDataSetDefault (&SettingsProvider);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_UNSUPPORTED);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConfDataSet of ConfDataSettingProvider with normal input.

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
ConfDataSetNormal (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS          Status;
  CHAR16              *CompareUniPtr[KNOWN_GOOD_TAG_COUNT];
  UINTN               UniStrSize = SINGLE_CONF_DATA_ID_LEN * sizeof (CHAR16);
  UINTN               Index      = 0;
  DFCI_SETTING_FLAGS  Flags      = 0;
  // Minimal initialization to tag this provider instance
  DFCI_SETTING_PROVIDER  SettingsProvider = {
    .Id = DFCI_OEM_SETTING_ID__CONF
  };

  will_return_always (MockSetVariable, EFI_SUCCESS);

  for (Index = 0; Index < KNOWN_GOOD_TAG_COUNT; Index++) {
    CompareUniPtr[Index] = AllocatePool (UniStrSize);
    UnicodeSPrintAsciiFormat (CompareUniPtr[Index], UniStrSize, SINGLE_SETTING_PROVIDER_TEMPLATE, mKnownGoodTags[Index].Tag);
    expect_memory (MockSetVariable, VariableName, CompareUniPtr[Index], UniStrSize);
    expect_value (MockSetVariable, DataSize, mKnownGoodTags[Index].DataSize);
    expect_memory (MockSetVariable, Data, mKnownGoodTags[Index].Data, mKnownGoodTags[Index].DataSize);
  }

  Status = ConfDataSet (
             &SettingsProvider,
             sizeof (mKnown_Good_Config_Data),
             mKnown_Good_Config_Data,
             &Flags
             );
  UT_ASSERT_NOT_EFI_ERROR (Status);

  Index = 0;
  while (Index < KNOWN_GOOD_TAG_COUNT) {
    FreePool (CompareUniPtr[Index]);
    Index++;
  }

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConfDataSet of ConfDataSettingProvider with null provider.

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
ConfDataSetNull (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS          Status;
  DFCI_SETTING_FLAGS  Flags = 0;
  // Minimal initialization to tag this provider instance
  DFCI_SETTING_PROVIDER  SettingsProvider = {
    .Id = DFCI_OEM_SETTING_ID__CONF
  };

  Status = ConfDataSet (NULL, sizeof (mKnown_Good_Config_Data), mKnown_Good_Config_Data, &Flags);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = ConfDataSet (&SettingsProvider, sizeof (mKnown_Good_Config_Data), NULL, &Flags);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = ConfDataSet (&SettingsProvider, sizeof (mKnown_Good_Config_Data), mKnown_Good_Config_Data, NULL);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConfDataSet of ConfDataSettingProvider with mismatched provider.

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
ConfDataSetMismatch (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS          Status;
  DFCI_SETTING_FLAGS  Flags = 0;
  // Minimal initialization to tag this provider instance
  DFCI_SETTING_PROVIDER  SettingsProvider = {
    .Id = SINGLE_SETTING_PROVIDER_START
  };

  Status = ConfDataSet (&SettingsProvider, sizeof (mKnown_Good_Config_Data), mKnown_Good_Config_Data, &Flags);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_UNSUPPORTED);

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

  AsciiSPrint (ComparePtr, SINGLE_CONF_DATA_ID_LEN, SINGLE_SETTING_PROVIDER_TEMPLATE, KNOWN_GOOD_TAG_0xF0);

  // Minimal initialization to tag this provider instance
  SettingsProvider.Id = ComparePtr;

  will_return (GetSectionFromAnyFv, mKnown_Good_Config_Data);
  will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Config_Data));

  will_return (GetSectionFromAnyFv, mKnown_Good_Config_Data);
  will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Config_Data));

  Status = SingleConfDataGetDefault (&SettingsProvider, &Size, Data);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_BUFFER_TOO_SMALL);
  UT_ASSERT_EQUAL (Size, sizeof (mGood_Tag_0xF0));

  will_return (GetSectionFromAnyFv, mKnown_Good_Config_Data);
  will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Config_Data));

  will_return (GetSectionFromAnyFv, mKnown_Good_Config_Data);
  will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Config_Data));

  Data   = AllocatePool (Size);
  Status = SingleConfDataGetDefault (&SettingsProvider, &Size, Data);
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_MEM_EQUAL (Data, mGood_Tag_0xF0, sizeof (mGood_Tag_0xF0));

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

  SettingsProvider.Id = SINGLE_SETTING_PROVIDER_START;
  Status              = SingleConfDataGetDefault (&SettingsProvider, &Size, Data);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

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

  will_return (GetSectionFromAnyFv, mKnown_Good_Config_Data);
  will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Config_Data));

  will_return (GetSectionFromAnyFv, mKnown_Good_Config_Data);
  will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Config_Data));

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

  AsciiSPrint (ComparePtr, sizeof (ComparePtr), SINGLE_SETTING_PROVIDER_TEMPLATE, KNOWN_GOOD_TAG_0xF0);
  UnicodeSPrintAsciiFormat (CompareUniPtr, sizeof (CompareUniPtr), SINGLE_SETTING_PROVIDER_TEMPLATE, KNOWN_GOOD_TAG_0xF0);

  // Minimal initialization to tag this provider instance
  SettingsProvider.Id = ComparePtr;

  expect_memory (MockGetVariable, VariableName, CompareUniPtr, sizeof (CompareUniPtr));
  will_return (MockGetVariable, sizeof (mGood_Tag_0xF0));

  Status = SingleConfDataGet (&SettingsProvider, &Size, Data);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_BUFFER_TOO_SMALL);
  UT_ASSERT_EQUAL (Size, sizeof (mGood_Tag_0xF0));

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

  SettingsProvider.Id = SINGLE_SETTING_PROVIDER_START;
  Status              = SingleConfDataGet (&SettingsProvider, &Size, Data);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

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

  expect_memory (MockGetVariable, VariableName, CompareUniPtr, sizeof (CompareUniPtr));
  will_return (MockGetVariable, 0);
  will_return (MockGetVariable, NULL);
  will_return (MockGetVariable, EFI_NOT_FOUND);

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

  AsciiSPrint (ComparePtr, sizeof (ComparePtr), SINGLE_SETTING_PROVIDER_TEMPLATE, KNOWN_GOOD_TAG_0xF0);
  UnicodeSPrintAsciiFormat (CompareUniPtr, sizeof (CompareUniPtr), SINGLE_SETTING_PROVIDER_TEMPLATE, KNOWN_GOOD_TAG_0xF0);

  // Minimal initialization to tag this provider instance
  SettingsProvider.Id = ComparePtr;

  will_return (GetSectionFromAnyFv, mKnown_Good_Config_Data);
  will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Config_Data));

  will_return (GetSectionFromAnyFv, mKnown_Good_Config_Data);
  will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Config_Data));

  expect_memory (MockSetVariable, VariableName, CompareUniPtr, sizeof (CompareUniPtr));
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
    .Id = DFCI_OEM_SETTING_ID__CONF
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

  SettingsProvider.Id = SINGLE_SETTING_PROVIDER_START;
  Status              = SingleConfDataSetDefault (&SettingsProvider);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  SettingsProvider.Id = SINGLE_SETTING_PROVIDER_TEMPLATE;
  Status              = SingleConfDataSetDefault (&SettingsProvider);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

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

  will_return (GetSectionFromAnyFv, mKnown_Good_Config_Data);
  will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Config_Data));

  will_return (GetSectionFromAnyFv, mKnown_Good_Config_Data);
  will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Config_Data));

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

  AsciiSPrint (ComparePtr, sizeof (ComparePtr), SINGLE_SETTING_PROVIDER_TEMPLATE, KNOWN_GOOD_TAG_0xF0);
  UnicodeSPrintAsciiFormat (CompareUniPtr, sizeof (CompareUniPtr), SINGLE_SETTING_PROVIDER_TEMPLATE, KNOWN_GOOD_TAG_0xF0);

  // Minimal initialization to tag this provider instance
  SettingsProvider.Id = ComparePtr;

  expect_memory (MockSetVariable, VariableName, CompareUniPtr, sizeof (CompareUniPtr));
  expect_value (MockSetVariable, DataSize, sizeof (mGood_Tag_0xF0));
  expect_memory (MockSetVariable, Data, mGood_Tag_0xF0, sizeof (mGood_Tag_0xF0));
  will_return (MockSetVariable, EFI_SUCCESS);

  Status = SingleConfDataSet (&SettingsProvider, sizeof (mGood_Tag_0xF0), mGood_Tag_0xF0, &Flags);
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
  Unit test for RuntimeDataSet of ConfDataSettingProvider with normal input.

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
RuntimeDataSetNormal (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS          Status;
  UINTN               Index = 0;
  DFCI_SETTING_FLAGS  Flags = 0;
  // Minimal initialization to tag this provider instance
  DFCI_SETTING_PROVIDER  SettingsProvider = {
    .Id = DFCI_OEM_SETTING_ID__RUNTIME
  };

  will_return_always (MockSetVariable, EFI_SUCCESS);

  for (Index = 0; Index < KNOWN_GOOD_RUNTIME_VAR_COUNT; Index++) {
    expect_memory (MockSetVariable, VariableName, mKnownGoodRuntimeVars[Index].Name, mKnownGoodRuntimeVars[Index].NameSize);
    expect_value (MockSetVariable, DataSize, mKnownGoodRuntimeVars[Index].DataSize);
    expect_memory (MockSetVariable, Data, mKnownGoodRuntimeVars[Index].Data, mKnownGoodRuntimeVars[Index].DataSize);
  }

  Status = RuntimeDataSet (
             &SettingsProvider,
             sizeof (mKnown_Good_Runtime_Data),
             mKnown_Good_Runtime_Data,
             &Flags
             );
  UT_ASSERT_NOT_EFI_ERROR (Status);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for RuntimeDataSet of ConfDataSettingProvider with null provider.

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
RuntimeDataSetNull (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS          Status;
  DFCI_SETTING_FLAGS  Flags = 0;
  // Minimal initialization to tag this provider instance
  DFCI_SETTING_PROVIDER  SettingsProvider = {
    .Id = DFCI_OEM_SETTING_ID__RUNTIME
  };

  Status = RuntimeDataSet (NULL, sizeof (mKnown_Good_Runtime_Data), mKnown_Good_Runtime_Data, &Flags);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = RuntimeDataSet (&SettingsProvider, sizeof (mKnown_Good_Runtime_Data), NULL, &Flags);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = RuntimeDataSet (&SettingsProvider, sizeof (mKnown_Good_Runtime_Data), mKnown_Good_Runtime_Data, NULL);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for RuntimeDataSet of ConfDataSettingProvider with mismatched provider.

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
RuntimeDataSetMismatch (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS          Status;
  DFCI_SETTING_FLAGS  Flags = 0;
  // Minimal initialization to tag this provider instance
  DFCI_SETTING_PROVIDER  SettingsProvider = {
    .Id = SINGLE_SETTING_PROVIDER_START
  };

  Status = RuntimeDataSet (&SettingsProvider, sizeof (mKnown_Good_Runtime_Data), mKnown_Good_Runtime_Data, &Flags);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_UNSUPPORTED);

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
  CHAR8   *ComparePtr[KNOWN_GOOD_TAG_COUNT];
  CHAR16  *CompareUniPtr[KNOWN_GOOD_TAG_COUNT];
  UINTN   Index = 0;

  expect_value (MockLocateProtocol, Protocol, &gDfciSettingsProviderSupportProtocolGuid);
  will_return (MockLocateProtocol, &mMockDfciSetting);

  expect_string (MockDfciRegisterProvider, Provider->Id, DFCI_OEM_SETTING_ID__CONF);
  expect_string (MockDfciRegisterProvider, Provider->Id, DFCI_OEM_SETTING_ID__RUNTIME);

  expect_value (MockLocateProtocol, Protocol, &gEdkiiVariablePolicyProtocolGuid);
  will_return (MockLocateProtocol, &mMockDfciSetting);

  will_return (GetSectionFromAnyFv, mKnown_Good_Config_Data);
  will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Config_Data));

  will_return (GetSectionFromAnyFv, mKnown_Good_Config_Data);
  will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Config_Data));

  for (Index = 0; Index < KNOWN_GOOD_TAG_COUNT - 1; Index++) {
    ComparePtr[Index] = AllocatePool (SINGLE_CONF_DATA_ID_LEN);
    AsciiSPrint (ComparePtr[Index], SINGLE_CONF_DATA_ID_LEN, SINGLE_SETTING_PROVIDER_TEMPLATE, mKnownGoodTags[Index].Tag);
    CompareUniPtr[Index] = AllocatePool (SINGLE_CONF_DATA_ID_LEN * sizeof (CHAR16));
    UnicodeSPrintAsciiFormat (CompareUniPtr[Index], SINGLE_CONF_DATA_ID_LEN * sizeof (CHAR16), SINGLE_SETTING_PROVIDER_TEMPLATE, mKnownGoodTags[Index].Tag);
    expect_string (MockDfciRegisterProvider, Provider->Id, ComparePtr[Index]);
    expect_memory (MockGetVariable, VariableName, CompareUniPtr[Index], SINGLE_CONF_DATA_ID_LEN * sizeof (CHAR16));
    will_return (MockGetVariable, mKnownGoodTags[Index].DataSize);
    expect_memory (RegisterVarStateVariablePolicy, Name, CompareUniPtr[Index], SINGLE_CONF_DATA_ID_LEN * sizeof (CHAR16));
    expect_value (RegisterVarStateVariablePolicy, MinSize, mKnownGoodTags[Index].DataSize);
  }

  // Intentionally leave out the last entry to make sure it will be set if first seen
  ComparePtr[Index] = AllocatePool (SINGLE_CONF_DATA_ID_LEN);
  AsciiSPrint (ComparePtr[Index], SINGLE_CONF_DATA_ID_LEN, SINGLE_SETTING_PROVIDER_TEMPLATE, mKnownGoodTags[Index].Tag);
  CompareUniPtr[Index] = AllocatePool (SINGLE_CONF_DATA_ID_LEN * sizeof (CHAR16));
  UnicodeSPrintAsciiFormat (CompareUniPtr[Index], SINGLE_CONF_DATA_ID_LEN * sizeof (CHAR16), SINGLE_SETTING_PROVIDER_TEMPLATE, mKnownGoodTags[Index].Tag);
  expect_string (MockDfciRegisterProvider, Provider->Id, ComparePtr[Index]);
  expect_memory (MockGetVariable, VariableName, CompareUniPtr[Index], SINGLE_CONF_DATA_ID_LEN * sizeof (CHAR16));
  will_return (MockGetVariable, 0);
  will_return (MockGetVariable, NULL);
  will_return (MockGetVariable, EFI_NOT_FOUND);
  expect_memory (MockSetVariable, VariableName, CompareUniPtr[Index], SINGLE_CONF_DATA_ID_LEN * sizeof (CHAR16));
  expect_value (MockSetVariable, DataSize, mKnownGoodTags[Index].DataSize);
  expect_memory (MockSetVariable, Data, mKnownGoodTags[Index].Data, mKnownGoodTags[Index].DataSize);
  will_return (MockSetVariable, EFI_SUCCESS);
  expect_memory (RegisterVarStateVariablePolicy, Name, CompareUniPtr[Index], SINGLE_CONF_DATA_ID_LEN * sizeof (CHAR16));
  expect_value (RegisterVarStateVariablePolicy, MinSize, mKnownGoodTags[Index].DataSize);

  SettingsProviderSupportProtocolNotify (NULL, NULL);

  Index = 0;
  while (Index < KNOWN_GOOD_TAG_COUNT) {
    FreePool (ComparePtr[Index]);
    FreePool (CompareUniPtr[Index]);
    Index++;
  }

  return UNIT_TEST_PASSED;
}

/**
  Unit test for GetTagIdFromDfciId of ConfDataSettingProvider.

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
GetTagIdFromDfciIdShouldComplete (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  CHAR8       ComparePtr[SINGLE_CONF_DATA_ID_LEN];
  UINT32      Tag;

  AsciiSPrint (ComparePtr, SINGLE_CONF_DATA_ID_LEN, SINGLE_SETTING_PROVIDER_TEMPLATE, KNOWN_GOOD_TAG_0xF0);

  Status = GetTagIdFromDfciId (ComparePtr, &Tag);
  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_EQUAL (Tag, KNOWN_GOOD_TAG_0xF0);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for GetTagIdFromDfciId of ConfDataSettingProvider with NULL.

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
GetTagIdFromDfciIdShouldFailNull (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  CHAR8       ComparePtr[SINGLE_CONF_DATA_ID_LEN];
  UINT32      Tag;

  AsciiSPrint (ComparePtr, SINGLE_CONF_DATA_ID_LEN, SINGLE_SETTING_PROVIDER_TEMPLATE, KNOWN_GOOD_TAG_0xF0);

  Status = GetTagIdFromDfciId (NULL, &Tag);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = GetTagIdFromDfciId (ComparePtr, NULL);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for GetTagIdFromDfciId of ConfDataSettingProvider with unformatted
  ID string.

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
GetTagIdFromDfciIdShouldFailOnMalformatted (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINT32      Tag;
  CHAR8       ComparePtr[SINGLE_CONF_DATA_ID_LEN];

  AsciiSPrint (ComparePtr, SINGLE_CONF_DATA_ID_LEN, "%08X%a", KNOWN_GOOD_TAG_0xF0, SINGLE_SETTING_PROVIDER_START);
  Status = GetTagIdFromDfciId (ComparePtr, &Tag);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  AsciiSPrint (ComparePtr, SINGLE_CONF_DATA_ID_LEN, "%a%%08X", SINGLE_SETTING_PROVIDER_START);
  Status = GetTagIdFromDfciId (ComparePtr, &Tag);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  AsciiSPrint (ComparePtr, SINGLE_CONF_DATA_ID_LEN, "%a08Xa", SINGLE_SETTING_PROVIDER_START);
  Status = GetTagIdFromDfciId (ComparePtr, &Tag);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for GetTagIdFromDfciId of ConfDataSettingProvider with unformatted
  ID string.

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
GetTagIdFromDfciIdShouldFailOnUnformatted (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINT32      Tag;

  Status = GetTagIdFromDfciId (DFCI_OEM_SETTING_ID__CONF, &Tag);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = GetTagIdFromDfciId (SINGLE_SETTING_PROVIDER_TEMPLATE, NULL);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  Status = GetTagIdFromDfciId (SINGLE_SETTING_PROVIDER_START, NULL);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

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
  AddTestCase (MiscTests, "Protocol notify routine should succeed", "ProtocolNotify", SettingsProviderNotifyShouldComplete, NULL, NULL, NULL);
  AddTestCase (MiscTests, "Get Tag ID from ascii string should succeed", "GetTagNormal", GetTagIdFromDfciIdShouldComplete, NULL, NULL, NULL);
  AddTestCase (MiscTests, "Get Tag ID from ascii string should fail on NULL pointers", "GetTagNull", GetTagIdFromDfciIdShouldFailNull, NULL, NULL, NULL);
  AddTestCase (MiscTests, "Get Tag ID from ascii string should fail on unformatted string", "GetTagUnformatted", GetTagIdFromDfciIdShouldFailOnUnformatted, NULL, NULL, NULL);
  AddTestCase (MiscTests, "Get Tag ID from ascii string should fail on mail formatted string", "GetTagMalformatted", GetTagIdFromDfciIdShouldFailOnMalformatted, NULL, NULL, NULL);

  AddTestCase (ConfSettingTests, "Get ConfData Default should return full blob", "GetFullDefaultNormal", ConfDataGetDefaultNormal, NULL, NULL, NULL);
  AddTestCase (ConfSettingTests, "Get ConfData Default should fail with null provider", "GetFullDefaultNull", ConfDataGetDefaultNull, NULL, NULL, NULL);
  AddTestCase (ConfSettingTests, "Get ConfData Default should fail with mismatched provider", "GetFullDefaultMismatch", ConfDataGetDefaultMismatch, NULL, NULL, NULL);

  AddTestCase (ConfSettingTests, "Get ConfData should return full blob", "GetFullNormal", ConfDataGetNormal, NULL, NULL, NULL);
  AddTestCase (ConfSettingTests, "Get ConfData should fail with null provider", "GetFullNull", ConfDataGetNull, NULL, NULL, NULL);
  AddTestCase (ConfSettingTests, "Get ConfData should fail with mismatched provider", "GetFullMismatch", ConfDataGetMismatch, NULL, NULL, NULL);

  AddTestCase (ConfSettingTests, "Set ConfData Default should return full blob", "GetFullDefaultNormal", ConfDataSetDefaultNormal, NULL, NULL, NULL);
  AddTestCase (ConfSettingTests, "Set ConfData Default should fail with null provider", "GetFullDefaultNull", ConfDataSetDefaultNull, NULL, NULL, NULL);
  AddTestCase (ConfSettingTests, "Set ConfData Default should fail with mismatched provider", "GetFullDefaultMismatch", ConfDataSetDefaultMismatch, NULL, NULL, NULL);

  AddTestCase (ConfSettingTests, "Set ConfData should return full blob", "SetFullNormal", ConfDataSetNormal, NULL, NULL, NULL);
  AddTestCase (ConfSettingTests, "Set ConfData should fail with null provider", "SetFullNull", ConfDataSetNull, NULL, NULL, NULL);
  AddTestCase (ConfSettingTests, "Set ConfData should fail with mismatched provider", "SetFullMismatch", ConfDataSetMismatch, NULL, NULL, NULL);

  AddTestCase (ConfSettingTests, "Set RuntimeData should return full blob", "SetFullNormal", RuntimeDataSetNormal, NULL, NULL, NULL);
  AddTestCase (ConfSettingTests, "Set RuntimeData should fail with null provider", "SetFullNull", RuntimeDataSetNull, NULL, NULL, NULL);
  AddTestCase (ConfSettingTests, "Set RuntimeData should fail with mismatched provider", "SetFullMismatch", RuntimeDataSetMismatch, NULL, NULL, NULL);

  AddTestCase (SingleSettingTests, "Get Single ConfData Default should return full blob", "SingleGetFullDefaultNormal", SingleConfDataGetDefaultNormal, NULL, NULL, NULL);
  AddTestCase (SingleSettingTests, "Get Single ConfData Default should fail with null provider", "SingleGetFullDefaultNull", SingleConfDataGetDefaultNull, NULL, NULL, NULL);
  AddTestCase (SingleSettingTests, "Get Single ConfData Default should fail with provider of invalid ID", "SingleGetFullDefaultInvalidId", SingleConfDataGetDefaultInvalidId, NULL, NULL, NULL);
  AddTestCase (SingleSettingTests, "Get Single ConfData Default should fail with provider of ID not related to config settings", "SingleGetDefaultNonTarget", SingleConfDataGetDefaultNonTarget, NULL, NULL, NULL);

  AddTestCase (SingleSettingTests, "Get Single ConfData should return full blob", "SingleGetFullNormal", SingleConfDataGetNormal, NULL, NULL, NULL);
  AddTestCase (SingleSettingTests, "Get Single ConfData should fail with null provider", "SingleGetFullNull", SingleConfDataGetNull, NULL, NULL, NULL);
  AddTestCase (SingleSettingTests, "Get Single ConfData should fail with provider of invalid ID", "SingleGetFullInvalidId", SingleConfDataGetInvalidId, NULL, NULL, NULL);
  AddTestCase (SingleSettingTests, "Get Single ConfData should fail with provider of ID not related to config settings", "SingleGetNonTarget", SingleConfDataGetNonTarget, NULL, NULL, NULL);

  AddTestCase (SingleSettingTests, "Set Single ConfData Default should return full blob", "SingleSetDefaultNormal", SingleConfDataSetDefaultNormal, NULL, NULL, NULL);
  AddTestCase (SingleSettingTests, "Set Single ConfData Default should fail with null provider", "SingleSetDefaultNull", SingleConfDataSetDefaultNull, NULL, NULL, NULL);
  AddTestCase (SingleSettingTests, "Set Single ConfData Default should fail with provider of invalid ID", "SingleSetDefaultInvalidId", SingleConfDataSetDefaultInvalidId, NULL, NULL, NULL);
  AddTestCase (SingleSettingTests, "Set Single ConfData Default should fail with provider of ID not related to config settings", "SingleSetDefaultNonTarget", SingleConfDataSetDefaultNonTarget, NULL, NULL, NULL);

  AddTestCase (SingleSettingTests, "Set Single ConfData should return full blob", "SingleSetNormal", SingleConfDataSetNormal, NULL, NULL, NULL);
  AddTestCase (SingleSettingTests, "Set Single ConfData should fail with null provider", "SingleSetNull", SingleConfDataSetNull, NULL, NULL, NULL);
  AddTestCase (SingleSettingTests, "Set Single ConfData should fail with provider of ID not related to config settings", "SingleSetNonTarget", SingleConfDataSetNonTarget, NULL, NULL, NULL);

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
