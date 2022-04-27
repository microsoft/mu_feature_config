/** @file
  Unit tests of the DxeResetSystemLib instance of the ResetSystemLib class

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

#define UNIT_TEST_APP_NAME      "Conf Data Setting Provider Unit Tests"
#define UNIT_TEST_APP_VERSION   "1.0"

#define KNOWN_GOOD_TAG_0xF0   0xF0
#define KNOWN_GOOD_TAG_0x70   0x70
#define KNOWN_GOOD_TAG_0x280  0x280
#define KNOWN_GOOD_TAG_0x180  0x180
#define KNOWN_GOOD_TAG_0x200  0x200
#define KNOWN_GOOD_TAG_0x10   0x10
#define KNOWN_GOOD_TAG_0x80   0x80

#define SINGLE_CONF_DATA_ID_LEN (sizeof (SINGLE_SETTING_PROVIDER_START) + sizeof (UINT32) * 2)

/**
  Library design is such that a dependency on gDfciSettingsProviderSupportProtocolGuid
  is not desired.  So to resolve that a ProtocolNotify is used.

  This function gets triggered once on install and 2nd time when the Protocol gets installed.

  When the gDfciSettingsProviderSupportProtocolGuid protocol is available the function will
  loop thru all supported device disablement supported features (using PCD) and install the settings

  @param[in]  Event                 Event whose notification function is being invoked.
  @param[in]  Context               The pointer to the notification function's context, which is
                                    implementation-dependent.
**/
VOID
EFIAPI
SettingsProviderSupportProtocolNotify (
  IN  EFI_EVENT  Event,
  IN  VOID       *Context
  );

/**
  Searches all the availables firmware volumes and returns the first matching FFS section.

  This function searches all the firmware volumes for FFS files with an FFS filename specified by NameGuid.
  The order that the firmware volumes is searched is not deterministic. For each FFS file found a search
  is made for FFS sections of type SectionType. If the FFS file contains at least SectionInstance instances
  of the FFS section specified by SectionType, then the SectionInstance instance is returned in Buffer.
  Buffer is allocated using AllocatePool(), and the size of the allocated buffer is returned in Size.
  It is the caller's responsibility to use FreePool() to free the allocated buffer.
  See EFI_FIRMWARE_VOLUME2_PROTOCOL.ReadSection() for details on how sections
  are retrieved from an FFS file based on SectionType and SectionInstance.

  If SectionType is EFI_SECTION_TE, and the search with an FFS file fails,
  the search will be retried with a section type of EFI_SECTION_PE32.
  This function must be called with a TPL <= TPL_NOTIFY.

  If NameGuid is NULL, then ASSERT().
  If Buffer is NULL, then ASSERT().
  If Size is NULL, then ASSERT().


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
  VOID *ret_buf;

  assert_memory_equal (NameGuid, PcdGetPtr (PcdConfigPolicyVariableGuid), sizeof (EFI_GUID));
  assert_int_equal (SectionType, EFI_SECTION_RAW);
  assert_non_null (Buffer);
  assert_non_null (Size);

  ret_buf = (VOID*)mock();
  if (ret_buf != NULL) {
    *Size = (UINTN)mock();
    *Buffer = AllocateCopyPool (*Size, ret_buf);
  } else {
    return EFI_NOT_FOUND;
  }
  return EFI_SUCCESS;
}

/**
Registers a Setting Provider with the System Settings module

@param  This                 Protocol instance pointer.
@param  Provider             Provider pointer to register

@retval EFI_SUCCESS          The provider registered.
@retval ERROR                The provider could not be registered.

**/
EFI_STATUS
EFIAPI
MockDfciRegisterProvider (
  IN DFCI_SETTING_PROVIDER_SUPPORT_PROTOCOL       *This,
  IN DFCI_SETTING_PROVIDER                        *Provider
  )
{
  assert_non_null (This);
  assert_non_null (Provider);

  DEBUG ((DEBUG_INFO, "%a Registering ID %a\n", __FUNCTION__, Provider->Id));

  check_expected (Provider->Id);

  return EFI_SUCCESS;
}

// Mocked version of setting provider register protocol instance.
DFCI_SETTING_PROVIDER_SUPPORT_PROTOCOL mMockDfciSetting = {
  .RegisterProvider = MockDfciRegisterProvider
};

/**
  Creates and returns a notification event and registers that event with all the protocol
  instances specified by ProtocolGuid.

  This function causes the notification function to be executed for every protocol of type
  ProtocolGuid instance that exists in the system when this function is invoked. If there are
  no instances of ProtocolGuid in the handle database at the time this function is invoked,
  then the notification function is still executed one time. In addition, every time a protocol
  of type ProtocolGuid instance is installed or reinstalled, the notification function is also
  executed. This function returns the notification event that was created.
  If ProtocolGuid is NULL, then ASSERT().
  If NotifyTpl is not a legal TPL value, then ASSERT().
  If NotifyFunction is NULL, then ASSERT().
  If Registration is NULL, then ASSERT().


  @param  ProtocolGuid    Supplies GUID of the protocol upon whose installation the event is fired.
  @param  NotifyTpl       Supplies the task priority level of the event notifications.
  @param  NotifyFunction  Supplies the function to notify when the event is signaled.
  @param  NotifyContext   The context parameter to pass to NotifyFunction.
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

  DEBUG ((DEBUG_INFO, "%a Name: %s, GUID: %g\n", __FUNCTION__, VariableName, VendorGuid));

  assert_non_null (VariableName);
  assert_memory_equal (VendorGuid, PcdGetPtr (PcdConfigPolicyVariableGuid), sizeof (EFI_GUID));
  assert_non_null (DataSize);

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
//   Unit test for ColdReset () API of the ResetSystemLib.

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
//   Unit test for WarmReset () API of the ResetSystemLib.

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
//   Unit test for ResetShutdown () API of the ResetSystemLib.

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

// /**
//   Unit test for ResetPlatformSpecific () API of the ResetSystemLib.

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
// ResetPlatformSpecificShouldIssueAPlatformSpecificReset (
//   IN UNIT_TEST_CONTEXT  Context
//   )
// {
//   expect_value (MockResetSystem, ResetType, EfiResetPlatformSpecific);
//   expect_value (MockResetSystem, ResetStatus, EFI_SUCCESS);

//   ResetPlatformSpecific (0, NULL);

//   return UNIT_TEST_PASSED;
// }

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
  CHAR8   ConfigId[SINGLE_CONF_DATA_ID_LEN];
  CHAR16  ConfigIdUni[SINGLE_CONF_DATA_ID_LEN];
  CHAR8   *ComparePtr[7];
  CHAR16  *CompareUniPtr[7];
  UINTN   Index = 0;

  expect_value (MockLocateProtocol, Protocol, &gDfciSettingsProviderSupportProtocolGuid);
  will_return (MockLocateProtocol, &mMockDfciSetting);

  expect_string (MockDfciRegisterProvider, Provider->Id, DFCI_OEM_SETTING_ID__CONF);

  expect_value (MockLocateProtocol, Protocol, &gEdkiiVariablePolicyProtocolGuid);
  will_return (MockLocateProtocol, &mMockDfciSetting);

  will_return (GetSectionFromAnyFv, mKnown_Good_Config_Data);
  will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Config_Data));

  will_return (GetSectionFromAnyFv, mKnown_Good_Config_Data);
  will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Config_Data));

  AsciiSPrint (ConfigId, sizeof (ConfigId), SINGLE_SETTING_PROVIDER_TEMPLATE, KNOWN_GOOD_TAG_0xF0);
  UnicodeSPrintAsciiFormat (ConfigIdUni, sizeof (ConfigIdUni), SINGLE_SETTING_PROVIDER_TEMPLATE, KNOWN_GOOD_TAG_0xF0);
  ComparePtr[Index] = AllocateCopyPool (sizeof (ConfigId), ConfigId);
  CompareUniPtr[Index] = AllocateCopyPool (sizeof (ConfigIdUni), ConfigIdUni);
  expect_string (MockDfciRegisterProvider, Provider->Id, ComparePtr[Index]);
  expect_memory (MockGetVariable, VariableName, CompareUniPtr[Index], sizeof (ConfigIdUni));
  will_return (MockGetVariable, sizeof (mGood_Tag_0xF0));
  expect_memory (RegisterVarStateVariablePolicy, Name, CompareUniPtr[Index], sizeof (ConfigIdUni));
  expect_value (RegisterVarStateVariablePolicy, MinSize, sizeof (mGood_Tag_0xF0));

  Index++;
  AsciiSPrint (ConfigId, sizeof (ConfigId), SINGLE_SETTING_PROVIDER_TEMPLATE, KNOWN_GOOD_TAG_0x70);
  UnicodeSPrintAsciiFormat (ConfigIdUni, sizeof (ConfigIdUni), SINGLE_SETTING_PROVIDER_TEMPLATE, KNOWN_GOOD_TAG_0x70);
  ComparePtr[Index] = AllocateCopyPool (sizeof (ConfigId), ConfigId);
  CompareUniPtr[Index] = AllocateCopyPool (sizeof (ConfigIdUni), ConfigIdUni);
  expect_string (MockDfciRegisterProvider, Provider->Id, ComparePtr[Index]);
  expect_memory (MockGetVariable, VariableName, CompareUniPtr[Index], sizeof (ConfigIdUni));
  will_return (MockGetVariable, sizeof (mGood_Tag_0x70));
  expect_memory (RegisterVarStateVariablePolicy, Name, CompareUniPtr[Index], sizeof (ConfigIdUni));
  expect_value (RegisterVarStateVariablePolicy, MinSize, sizeof (mGood_Tag_0x70));

  Index++;
  AsciiSPrint (ConfigId, sizeof (ConfigId), SINGLE_SETTING_PROVIDER_TEMPLATE, KNOWN_GOOD_TAG_0x280);
  UnicodeSPrintAsciiFormat (ConfigIdUni, sizeof (ConfigIdUni), SINGLE_SETTING_PROVIDER_TEMPLATE, KNOWN_GOOD_TAG_0x280);
  ComparePtr[Index] = AllocateCopyPool (sizeof (ConfigId), ConfigId);
  CompareUniPtr[Index] = AllocateCopyPool (sizeof (ConfigIdUni), ConfigIdUni);
  expect_string (MockDfciRegisterProvider, Provider->Id, ComparePtr[Index]);
  expect_memory (MockGetVariable, VariableName, CompareUniPtr[Index], sizeof (ConfigIdUni));
  will_return (MockGetVariable, sizeof (mGood_Tag_0x280));
  expect_memory (RegisterVarStateVariablePolicy, Name, CompareUniPtr[Index], sizeof (ConfigIdUni));
  expect_value (RegisterVarStateVariablePolicy, MinSize, sizeof (mGood_Tag_0x280));

  Index++;
  AsciiSPrint (ConfigId, sizeof (ConfigId), SINGLE_SETTING_PROVIDER_TEMPLATE, KNOWN_GOOD_TAG_0x180);
  UnicodeSPrintAsciiFormat (ConfigIdUni, sizeof (ConfigIdUni), SINGLE_SETTING_PROVIDER_TEMPLATE, KNOWN_GOOD_TAG_0x180);
  ComparePtr[Index] = AllocateCopyPool (sizeof (ConfigId), ConfigId);
  CompareUniPtr[Index] = AllocateCopyPool (sizeof (ConfigIdUni), ConfigIdUni);
  expect_string (MockDfciRegisterProvider, Provider->Id, ComparePtr[Index]);
  expect_memory (MockGetVariable, VariableName, CompareUniPtr[Index], sizeof (ConfigIdUni));
  will_return (MockGetVariable, sizeof (mGood_Tag_0x180));
  expect_memory (RegisterVarStateVariablePolicy, Name, CompareUniPtr[Index], sizeof (ConfigIdUni));
  expect_value (RegisterVarStateVariablePolicy, MinSize, sizeof (mGood_Tag_0x180));

  Index++;
  AsciiSPrint (ConfigId, sizeof (ConfigId), SINGLE_SETTING_PROVIDER_TEMPLATE, KNOWN_GOOD_TAG_0x200);
  UnicodeSPrintAsciiFormat (ConfigIdUni, sizeof (ConfigIdUni), SINGLE_SETTING_PROVIDER_TEMPLATE, KNOWN_GOOD_TAG_0x200);
  ComparePtr[Index] = AllocateCopyPool (sizeof (ConfigId), ConfigId);
  CompareUniPtr[Index] = AllocateCopyPool (sizeof (ConfigIdUni), ConfigIdUni);
  expect_string (MockDfciRegisterProvider, Provider->Id, ComparePtr[Index]);
  expect_memory (MockGetVariable, VariableName, CompareUniPtr[Index], sizeof (ConfigIdUni));
  will_return (MockGetVariable, sizeof (mGood_Tag_0x200));
  expect_memory (RegisterVarStateVariablePolicy, Name, CompareUniPtr[Index], sizeof (ConfigIdUni));
  expect_value (RegisterVarStateVariablePolicy, MinSize, sizeof (mGood_Tag_0x200));

  Index++;
  AsciiSPrint (ConfigId, sizeof (ConfigId), SINGLE_SETTING_PROVIDER_TEMPLATE, KNOWN_GOOD_TAG_0x10);
  UnicodeSPrintAsciiFormat (ConfigIdUni, sizeof (ConfigIdUni), SINGLE_SETTING_PROVIDER_TEMPLATE, KNOWN_GOOD_TAG_0x10);
  ComparePtr[Index] = AllocateCopyPool (sizeof (ConfigId), ConfigId);
  CompareUniPtr[Index] = AllocateCopyPool (sizeof (ConfigIdUni), ConfigIdUni);
  expect_string (MockDfciRegisterProvider, Provider->Id, ComparePtr[Index]);
  expect_memory (MockGetVariable, VariableName, CompareUniPtr[Index], sizeof (ConfigIdUni));
  will_return (MockGetVariable, sizeof (mGood_Tag_0x10));
  expect_memory (RegisterVarStateVariablePolicy, Name, CompareUniPtr[Index], sizeof (ConfigIdUni));
  expect_value (RegisterVarStateVariablePolicy, MinSize, sizeof (mGood_Tag_0x10));

  Index++;
  AsciiSPrint (ConfigId, sizeof (ConfigId), SINGLE_SETTING_PROVIDER_TEMPLATE, KNOWN_GOOD_TAG_0x80);
  UnicodeSPrintAsciiFormat (ConfigIdUni, sizeof (ConfigIdUni), SINGLE_SETTING_PROVIDER_TEMPLATE, KNOWN_GOOD_TAG_0x80);
  ComparePtr[Index] = AllocateCopyPool (sizeof (ConfigId), ConfigId);
  CompareUniPtr[Index] = AllocateCopyPool (sizeof (ConfigIdUni), ConfigIdUni);
  expect_string (MockDfciRegisterProvider, Provider->Id, ComparePtr[Index]);
  expect_memory (MockGetVariable, VariableName, CompareUniPtr[Index], sizeof (ConfigIdUni));
  will_return (MockGetVariable, sizeof (mGood_Tag_0x80));
  expect_memory (RegisterVarStateVariablePolicy, Name, CompareUniPtr[Index], sizeof (ConfigIdUni));
  expect_value (RegisterVarStateVariablePolicy, MinSize, sizeof (mGood_Tag_0x80));

  SettingsProviderSupportProtocolNotify  (NULL, NULL);

  return UNIT_TEST_PASSED;
}

/**
  Initialze the unit test framework, suite, and unit tests for the
  ResetSystemLib and run the ResetSystemLib unit test.

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
  UNIT_TEST_SUITE_HANDLE      ResetTests;

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
  // Populate the ResetSytemLib Unit Test Suite.
  //
  Status = CreateUnitTestSuite (&ResetTests, Framework, "DxeResetSystemLib Reset Tests", "ResetSystemLib.Reset", NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in CreateUnitTestSuite for ResetTests\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  //
  // --------------Suite-----------Description--------------Name----------Function--------Pre---Post-------------------Context-----------
  //
  AddTestCase (ResetTests, "Protocol notify routine should succeed", "ProtocolNotify", SettingsProviderNotifyShouldComplete, NULL, NULL, NULL);
  // AddTestCase (ResetTests, "ResetWarm should issue a warm reset", "Warm", ResetWarmShouldIssueAWarmReset, NULL, NULL, NULL);
  // AddTestCase (ResetTests, "ResetShutdown should issue a shutdown", "Shutdown", ResetShutdownShouldIssueAShutdown, NULL, NULL, NULL);
  // AddTestCase (ResetTests, "ResetPlatformSpecific should issue a platform-specific reset", "Platform", ResetPlatformSpecificShouldIssueAPlatformSpecificReset, NULL, NULL, NULL);
  // AddTestCase (ResetTests, "ResetSystem should pass all parameters through", "Parameters", ResetSystemShouldPassTheParametersThrough, NULL, NULL, NULL);

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
