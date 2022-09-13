/** @file
  Unit tests of the ConfProfileMgrDxe module

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
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <PiDxe.h>
#include <Library/DxeServicesLib.h>
#include <Library/ConfigVariableListLib.h>
#include <Library/ActiveProfileSelectorLib.h>

#include <Library/UnitTestLib.h>
#include <Good_Config_Data.h>
#include "ConfProfileMgrDxeUnitTest.h"

#define UNIT_TEST_APP_NAME                 "Configuration Profile Manager DXE Unit Tests"
#define UNIT_TEST_APP_VERSION              "1.0"
#define CACHED_CONF_PROFILE_VARIABLE_NAME  L"CachedConfProfileGuid"

// Unused, needed for DxeServicesLib
EFI_HANDLE  gImageHandle = NULL;

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
  Mock Install Protocol Interface

  @param  UserHandle             The handle to install the protocol handler on,
                                 or NULL if a new handle is to be allocated
  @param  Protocol               The protocol to add to the handle
  @param  InterfaceType          Indicates whether Interface is supplied in
                                 native form.
  @param  Interface              The interface for the protocol being added

  @return Status code

**/
EFI_STATUS
EFIAPI
MockInstallProtocolInterface (
  IN OUT EFI_HANDLE      *UserHandle,
  IN EFI_GUID            *Protocol,
  IN EFI_INTERFACE_TYPE  InterfaceType,
  IN VOID                *Interface
  )
{
  assert_non_null (Protocol);
  assert_memory_equal (Protocol, &gConfProfileMgrProfileValidProtocolGuid, sizeof (EFI_GUID));

  return (EFI_STATUS)mock ();
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
  .InstallProtocolInterface = MockInstallProtocolInterface,  // LocateProtocol
};

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

// System table, not used in this test.
EFI_SYSTEM_TABLE  MockSys;

/**
  Unit test for ActiveProfileSelectorNull.

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
RetrieveActiveProfileGuidShouldMatch (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  EFI_GUID    Guid;

  will_return (LibPcdGetPtr, &gSetupDataPkgGenericProfileGuid);

  Status = RetrieveActiveProfileGuid (&Guid);

  UT_ASSERT_NOT_EFI_ERROR (Status);
  UT_ASSERT_MEM_EQUAL (&Guid, &gSetupDataPkgGenericProfileGuid, sizeof (Guid));

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ActiveProfileSelectorNull.

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
RetrieveActiveProfileGuidShouldFail (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  EFI_GUID    Guid;

  Status = RetrieveActiveProfileGuid (NULL);
  UT_ASSERT_STATUS_EQUAL (Status, EFI_INVALID_PARAMETER);

  will_return (LibPcdGetPtr, NULL);

  Status = RetrieveActiveProfileGuid (&Guid);

  UT_ASSERT_STATUS_EQUAL (Status, EFI_NO_RESPONSE);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConfProfileMgrDxe.

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
ConfProfileMgrDxeShouldUseRetrievedProfile (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINT32      i;

  // Getting cached variable, force it to write variable
  will_return (MockGetVariable, sizeof (EFI_GUID));
  will_return (MockGetVariable, &gZeroGuid);
  will_return (MockGetVariable, 3);
  will_return (MockGetVariable, EFI_SUCCESS);

  will_return (GetSectionFromAnyFv, mKnown_Good_Generic_Profile);
  will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Generic_Profile));
  will_return (LibPcdGetPtr, &gSetupDataPkgGenericProfileGuid);
  will_return (LibPcdGetPtr, &gSetupDataPkgGenericProfileGuid);

  will_return (LibPcdSetPtrS, EFI_SUCCESS);

  expect_memory (MockSetVariable, VariableName, CACHED_CONF_PROFILE_VARIABLE_NAME, StrSize (CACHED_CONF_PROFILE_VARIABLE_NAME));
  expect_memory (MockSetVariable, VendorGuid, &gConfProfileMgrVariableGuid, sizeof (EFI_GUID));
  expect_value (MockSetVariable, DataSize, sizeof (EFI_GUID));
  expect_memory (MockSetVariable, Data, &gSetupDataPkgGenericProfileGuid, sizeof (EFI_GUID));
  will_return (MockSetVariable, EFI_SUCCESS);

  // Cause profile to be validated
  will_return (IsSystemInManufacturingMode, FALSE);

  // All the GetVariable calls...
  for (i = 0; i < 9; i++) {
    will_return (MockGetVariable, mKnown_Good_VarList_DataSizes[i]);
    will_return (MockGetVariable, mKnown_Good_VarList_Entries[i]);

    if (i < 2) {
      will_return (MockGetVariable, 3);
    } else {
      // XML part of blob
      will_return (MockGetVariable, 7);
    }

    will_return (MockGetVariable, EFI_SUCCESS);
  }

  will_return (MockInstallProtocolInterface, EFI_SUCCESS);

  Status = ConfProfileMgrDxeEntry (NULL, NULL);
  UT_ASSERT_NOT_EFI_ERROR (Status);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConfProfileMgrDxe.

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
ConfProfileMgrDxeShouldUseCachedProfile (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINT32      i;

  // Getting cached variable, it should not write variable
  will_return (MockGetVariable, sizeof (EFI_GUID));
  will_return (MockGetVariable, &gSetupDataPkgGenericProfileGuid);
  will_return (MockGetVariable, 3);
  will_return (MockGetVariable, EFI_SUCCESS);

  will_return (GetSectionFromAnyFv, mKnown_Good_Generic_Profile);
  will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Generic_Profile));

  // Force bad profile to be retrieved
  will_return (LibPcdGetPtr, &gZeroGuid);
  will_return (LibPcdGetPtr, &gSetupDataPkgGenericProfileGuid);

  will_return (LibPcdSetPtrS, EFI_SUCCESS);

  // Cause profile to be validated
  will_return (IsSystemInManufacturingMode, FALSE);

  // All the GetVariable calls...
  for (i = 0; i < 9; i++) {
    will_return (MockGetVariable, mKnown_Good_VarList_DataSizes[i]);
    will_return (MockGetVariable, mKnown_Good_VarList_Entries[i]);

    if (i < 2) {
      will_return (MockGetVariable, 3);
    } else {
      // XML part of blob
      will_return (MockGetVariable, 7);
    }

    will_return (MockGetVariable, EFI_SUCCESS);
  }

  will_return (MockInstallProtocolInterface, EFI_SUCCESS);

  Status = ConfProfileMgrDxeEntry (NULL, NULL);
  UT_ASSERT_NOT_EFI_ERROR (Status);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConfProfileMgrDxe.

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
ConfProfileMgrDxeShouldUseGenericProfile (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  UINT32      i;

  // Fail to get cached variable
  will_return (MockGetVariable, sizeof (EFI_GUID));
  will_return (MockGetVariable, &gSetupDataPkgGenericProfileGuid);
  will_return (MockGetVariable, 3);
  will_return (MockGetVariable, EFI_NOT_FOUND);

  will_return (GetSectionFromAnyFv, mKnown_Good_Generic_Profile);
  will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Generic_Profile));

  // Force bad profile to be retrieved
  will_return (LibPcdGetPtr, &gZeroGuid);
  will_return (LibPcdGetPtr, &gSetupDataPkgGenericProfileGuid);

  will_return (LibPcdSetPtrS, EFI_SUCCESS);

  expect_memory (MockSetVariable, VariableName, CACHED_CONF_PROFILE_VARIABLE_NAME, StrSize (CACHED_CONF_PROFILE_VARIABLE_NAME));
  expect_memory (MockSetVariable, VendorGuid, &gConfProfileMgrVariableGuid, sizeof (EFI_GUID));
  expect_value (MockSetVariable, DataSize, sizeof (EFI_GUID));
  expect_memory (MockSetVariable, Data, &gSetupDataPkgGenericProfileGuid, sizeof (EFI_GUID));
  will_return (MockSetVariable, EFI_SUCCESS);

  // Cause profile to be validated
  will_return (IsSystemInManufacturingMode, FALSE);

  // All the GetVariable calls...
  for (i = 0; i < 9; i++) {
    will_return (MockGetVariable, mKnown_Good_VarList_DataSizes[i]);
    will_return (MockGetVariable, mKnown_Good_VarList_Entries[i]);

    if (i < 2) {
      will_return (MockGetVariable, 3);
    } else {
      // XML part of blob
      will_return (MockGetVariable, 7);
    }

    will_return (MockGetVariable, EFI_SUCCESS);
  }

  will_return (MockInstallProtocolInterface, EFI_SUCCESS);

  Status = ConfProfileMgrDxeEntry (NULL, NULL);
  UT_ASSERT_NOT_EFI_ERROR (Status);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConfProfileMgrDxe.

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
ConfProfileMgrDxeShouldAssert (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  UINT32  i;

  // Getting cached variable, it should not write variable
  will_return (MockGetVariable, sizeof (EFI_GUID));
  will_return (MockGetVariable, &gSetupDataPkgGenericProfileGuid);
  will_return (MockGetVariable, 3);
  will_return (MockGetVariable, EFI_SUCCESS);

  will_return (LibPcdGetPtr, &gSetupDataPkgGenericProfileGuid);

  // Make set PCD fail
  will_return (LibPcdSetPtrS, EFI_OUT_OF_RESOURCES);

  UT_EXPECT_ASSERT_FAILURE (ConfProfileMgrDxeEntry (NULL, NULL), NULL);

  // Getting cached variable, it should not write variable
  will_return (MockGetVariable, sizeof (EFI_GUID));
  will_return (MockGetVariable, &gSetupDataPkgGenericProfileGuid);
  will_return (MockGetVariable, 3);
  will_return (MockGetVariable, EFI_SUCCESS);

  // Force assert if SetVariable fails
  will_return (GetSectionFromAnyFv, mKnown_Good_Generic_Profile);
  will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Generic_Profile));
  will_return (LibPcdGetPtr, &gSetupDataPkgGenericProfileGuid);
  will_return (LibPcdGetPtr, &gSetupDataPkgGenericProfileGuid);

  will_return (LibPcdSetPtrS, EFI_SUCCESS);

  // Cause profile to be validated
  will_return (IsSystemInManufacturingMode, FALSE);

  // All the GetVariable calls...
  for (i = 0; i < 9; i++) {
    will_return (MockGetVariable, mKnown_Good_VarList_DataSizes[i]);
    will_return (MockGetVariable, mKnown_Good_VarList_Entries[i]);

    if (i < 2) {
      will_return (MockGetVariable, 3);
    } else {
      // XML part of blob
      will_return (MockGetVariable, 7);
    }

    if (i == 8) {
      will_return (MockGetVariable, EFI_NOT_FOUND);
    } else {
      will_return (MockGetVariable, EFI_SUCCESS);
    }
  }

  // delete var
  expect_memory (MockSetVariable, VariableName, mKnown_Good_VarList_Names[8], StrSize (mKnown_Good_VarList_Names[8]));
  expect_memory (MockSetVariable, VendorGuid, &mKnown_Good_Xml_Guid, sizeof (EFI_GUID));
  expect_value (MockSetVariable, DataSize, 0);
  expect_value (MockSetVariable, Data, NULL);
  will_return (MockSetVariable, EFI_SUCCESS);

  // set var
  expect_memory (MockSetVariable, VariableName, mKnown_Good_VarList_Names[8], StrSize (mKnown_Good_VarList_Names[8]));
  expect_memory (MockSetVariable, VendorGuid, &mKnown_Good_Xml_Guid, sizeof (EFI_GUID));
  expect_value (MockSetVariable, DataSize, mKnown_Good_VarList_DataSizes[8]);
  expect_memory (MockSetVariable, Data, mKnown_Good_VarList_Entries[8], mKnown_Good_VarList_DataSizes[8]);
  will_return (MockSetVariable, EFI_NOT_FOUND);

  UT_EXPECT_ASSERT_FAILURE (ConfProfileMgrDxeEntry (NULL, NULL), NULL);

  // Getting cached variable, it should not write variable
  will_return (MockGetVariable, sizeof (EFI_GUID));
  will_return (MockGetVariable, &gSetupDataPkgGenericProfileGuid);
  will_return (MockGetVariable, 3);
  will_return (MockGetVariable, EFI_SUCCESS);

  // Force assert in Protocol Install failure
  will_return (GetSectionFromAnyFv, mKnown_Good_Generic_Profile);
  will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Generic_Profile));
  will_return (LibPcdGetPtr, &gSetupDataPkgGenericProfileGuid);
  will_return (LibPcdGetPtr, &gSetupDataPkgGenericProfileGuid);

  will_return (LibPcdSetPtrS, EFI_SUCCESS);

  // Cause profile to be validated
  will_return (IsSystemInManufacturingMode, FALSE);

  // All the GetVariable calls...
  for (i = 0; i < 9; i++) {
    will_return (MockGetVariable, mKnown_Good_VarList_DataSizes[i]);
    will_return (MockGetVariable, mKnown_Good_VarList_Entries[i]);

    if (i < 2) {
      will_return (MockGetVariable, 3);
    } else {
      // XML part of blob
      will_return (MockGetVariable, 7);
    }

    will_return (MockGetVariable, EFI_SUCCESS);
  }

  // Fail to set protocol
  will_return (MockInstallProtocolInterface, EFI_OUT_OF_RESOURCES);

  UT_EXPECT_ASSERT_FAILURE (ConfProfileMgrDxeEntry (NULL, NULL), NULL);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConfProfileMgrDxe.

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
ConfProfileMgrDxeShouldWriteReceivedProfileAndReset (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                Status;
  UINT32                    i;
  BASE_LIBRARY_JUMP_BUFFER  JumpBuf;

  // Getting cached variable, it should write variable
  will_return (MockGetVariable, sizeof (EFI_GUID));
  will_return (MockGetVariable, &gZeroGuid);
  will_return (MockGetVariable, 3);
  will_return (MockGetVariable, EFI_SUCCESS);

  will_return (GetSectionFromAnyFv, mKnown_Good_Generic_Profile);
  will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Generic_Profile));
  will_return (LibPcdGetPtr, &gSetupDataPkgGenericProfileGuid);
  will_return (LibPcdGetPtr, &gSetupDataPkgGenericProfileGuid);

  will_return (LibPcdSetPtrS, EFI_SUCCESS);

  expect_memory (MockSetVariable, VariableName, CACHED_CONF_PROFILE_VARIABLE_NAME, StrSize (CACHED_CONF_PROFILE_VARIABLE_NAME));
  expect_memory (MockSetVariable, VendorGuid, &gConfProfileMgrVariableGuid, sizeof (EFI_GUID));
  expect_value (MockSetVariable, DataSize, sizeof (EFI_GUID));
  expect_memory (MockSetVariable, Data, &gSetupDataPkgGenericProfileGuid, sizeof (EFI_GUID));
  will_return (MockSetVariable, EFI_SUCCESS);

  // Cause profile to be validated
  will_return (IsSystemInManufacturingMode, FALSE);

  // Force profile to not match flash with bad entry
  will_return (MockGetVariable, mKnown_Good_VarList_DataSizes[0]);
  will_return (MockGetVariable, mKnown_Good_VarList_Entries[1]);

  will_return (MockGetVariable, 3);
  will_return (MockGetVariable, EFI_SUCCESS);

  // set var
  expect_memory (MockSetVariable, VariableName, mKnown_Good_VarList_Names[0], StrSize (mKnown_Good_VarList_Names[0]));
  expect_memory (MockSetVariable, VendorGuid, &mKnown_Good_Yaml_Guid, sizeof (EFI_GUID));
  expect_value (MockSetVariable, DataSize, mKnown_Good_VarList_DataSizes[0]);
  expect_memory (MockSetVariable, Data, mKnown_Good_VarList_Entries[0], mKnown_Good_VarList_DataSizes[0]);
  will_return (MockSetVariable, EFI_SUCCESS);

  for (i = 1; i < 7; i++) {
    will_return (MockGetVariable, mKnown_Good_VarList_DataSizes[i]);
    will_return (MockGetVariable, mKnown_Good_VarList_Entries[i]);

    if (i < 2) {
      will_return (MockGetVariable, 3);
    } else {
      // XML part of blob
      will_return (MockGetVariable, 7);
    }

    will_return (MockGetVariable, EFI_SUCCESS);
  }

  // Force profile to not match flash with bad datasize
  will_return (MockGetVariable, 140);

  // delete var
  expect_memory (MockSetVariable, VariableName, mKnown_Good_VarList_Names[7], StrSize (mKnown_Good_VarList_Names[7]));
  expect_memory (MockSetVariable, VendorGuid, &mKnown_Good_Xml_Guid, sizeof (EFI_GUID));
  expect_value (MockSetVariable, DataSize, 0);
  expect_value (MockSetVariable, Data, NULL);
  will_return (MockSetVariable, EFI_SUCCESS);

  // set var
  expect_memory (MockSetVariable, VariableName, mKnown_Good_VarList_Names[7], StrSize (mKnown_Good_VarList_Names[7]));
  expect_memory (MockSetVariable, VendorGuid, &mKnown_Good_Xml_Guid, sizeof (EFI_GUID));
  expect_value (MockSetVariable, DataSize, mKnown_Good_VarList_DataSizes[7]);
  expect_memory (MockSetVariable, Data, mKnown_Good_VarList_Entries[7], mKnown_Good_VarList_DataSizes[7]);
  will_return (MockSetVariable, EFI_SUCCESS);

  // Force profile to not match flash with bad attribute
  will_return (MockGetVariable, mKnown_Good_VarList_DataSizes[8]);
  will_return (MockGetVariable, mKnown_Good_VarList_Entries[8]);

  will_return (MockGetVariable, 2);
  will_return (MockGetVariable, EFI_SUCCESS);

  // delete var
  expect_memory (MockSetVariable, VariableName, mKnown_Good_VarList_Names[8], StrSize (mKnown_Good_VarList_Names[8]));
  expect_memory (MockSetVariable, VendorGuid, &mKnown_Good_Xml_Guid, sizeof (EFI_GUID));
  expect_value (MockSetVariable, DataSize, 0);
  expect_value (MockSetVariable, Data, NULL);
  will_return (MockSetVariable, EFI_SUCCESS);

  // set var
  expect_memory (MockSetVariable, VariableName, mKnown_Good_VarList_Names[8], StrSize (mKnown_Good_VarList_Names[8]));
  expect_memory (MockSetVariable, VendorGuid, &mKnown_Good_Xml_Guid, sizeof (EFI_GUID));
  expect_value (MockSetVariable, DataSize, mKnown_Good_VarList_DataSizes[8]);
  expect_memory (MockSetVariable, Data, mKnown_Good_VarList_Entries[8], mKnown_Good_VarList_DataSizes[8]);
  will_return (MockSetVariable, EFI_SUCCESS);

  expect_memory (ResetSystemWithSubtype, ResetSubtype, &gConfProfileMgrResetGuid, sizeof (EFI_GUID));
  will_return (ResetSystemWithSubtype, &JumpBuf);

  if (!SetJump (&JumpBuf)) {
    Status = ConfProfileMgrDxeEntry (NULL, NULL);
    UT_ASSERT_NOT_EFI_ERROR (Status);
  }

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConfProfileMgrDxe.

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
ConfProfileMgrDxeShouldWriteCachedProfileAndReset (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                Status;
  UINT32                    i;
  BASE_LIBRARY_JUMP_BUFFER  JumpBuf;

  // Getting cached variable, it should not write variable
  will_return (MockGetVariable, sizeof (EFI_GUID));
  will_return (MockGetVariable, &gSetupDataPkgGenericProfileGuid);
  will_return (MockGetVariable, 3);
  will_return (MockGetVariable, EFI_SUCCESS);

  will_return (GetSectionFromAnyFv, mKnown_Good_Generic_Profile);
  will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Generic_Profile));

  // Force bad profile to be retrieved
  will_return (LibPcdGetPtr, &gZeroGuid);
  will_return (LibPcdGetPtr, &gSetupDataPkgGenericProfileGuid);

  will_return (LibPcdSetPtrS, EFI_SUCCESS);

  // Cause profile to be validated
  will_return (IsSystemInManufacturingMode, FALSE);

  // Force profile to not match flash with bad entry
  will_return (MockGetVariable, mKnown_Good_VarList_DataSizes[0]);
  will_return (MockGetVariable, mKnown_Good_VarList_Entries[1]);

  will_return (MockGetVariable, 3);
  will_return (MockGetVariable, EFI_SUCCESS);

  // set var
  expect_memory (MockSetVariable, VariableName, mKnown_Good_VarList_Names[0], StrSize (mKnown_Good_VarList_Names[0]));
  expect_memory (MockSetVariable, VendorGuid, &mKnown_Good_Yaml_Guid, sizeof (EFI_GUID));
  expect_value (MockSetVariable, DataSize, mKnown_Good_VarList_DataSizes[0]);
  expect_memory (MockSetVariable, Data, mKnown_Good_VarList_Entries[0], mKnown_Good_VarList_DataSizes[0]);
  will_return (MockSetVariable, EFI_SUCCESS);

  for (i = 1; i < 7; i++) {
    will_return (MockGetVariable, mKnown_Good_VarList_DataSizes[i]);
    will_return (MockGetVariable, mKnown_Good_VarList_Entries[i]);

    if (i < 2) {
      will_return (MockGetVariable, 3);
    } else {
      // XML part of blob
      will_return (MockGetVariable, 7);
    }

    will_return (MockGetVariable, EFI_SUCCESS);
  }

  // Force profile to not match flash with bad datasize
  will_return (MockGetVariable, 140);

  // delete var
  expect_memory (MockSetVariable, VariableName, mKnown_Good_VarList_Names[7], StrSize (mKnown_Good_VarList_Names[7]));
  expect_memory (MockSetVariable, VendorGuid, &mKnown_Good_Xml_Guid, sizeof (EFI_GUID));
  expect_value (MockSetVariable, DataSize, 0);
  expect_value (MockSetVariable, Data, NULL);
  will_return (MockSetVariable, EFI_SUCCESS);

  // set var
  expect_memory (MockSetVariable, VariableName, mKnown_Good_VarList_Names[7], StrSize (mKnown_Good_VarList_Names[7]));
  expect_memory (MockSetVariable, VendorGuid, &mKnown_Good_Xml_Guid, sizeof (EFI_GUID));
  expect_value (MockSetVariable, DataSize, mKnown_Good_VarList_DataSizes[7]);
  expect_memory (MockSetVariable, Data, mKnown_Good_VarList_Entries[7], mKnown_Good_VarList_DataSizes[7]);
  will_return (MockSetVariable, EFI_SUCCESS);

  // Force profile to not match flash with bad attribute
  will_return (MockGetVariable, mKnown_Good_VarList_DataSizes[8]);
  will_return (MockGetVariable, mKnown_Good_VarList_Entries[8]);

  will_return (MockGetVariable, 2);
  will_return (MockGetVariable, EFI_SUCCESS);

  // delete var
  expect_memory (MockSetVariable, VariableName, mKnown_Good_VarList_Names[8], StrSize (mKnown_Good_VarList_Names[8]));
  expect_memory (MockSetVariable, VendorGuid, &mKnown_Good_Xml_Guid, sizeof (EFI_GUID));
  expect_value (MockSetVariable, DataSize, 0);
  expect_value (MockSetVariable, Data, NULL);
  will_return (MockSetVariable, EFI_SUCCESS);

  // set var
  expect_memory (MockSetVariable, VariableName, mKnown_Good_VarList_Names[8], StrSize (mKnown_Good_VarList_Names[8]));
  expect_memory (MockSetVariable, VendorGuid, &mKnown_Good_Xml_Guid, sizeof (EFI_GUID));
  expect_value (MockSetVariable, DataSize, mKnown_Good_VarList_DataSizes[8]);
  expect_memory (MockSetVariable, Data, mKnown_Good_VarList_Entries[8], mKnown_Good_VarList_DataSizes[8]);
  will_return (MockSetVariable, EFI_SUCCESS);

  expect_memory (ResetSystemWithSubtype, ResetSubtype, &gConfProfileMgrResetGuid, sizeof (EFI_GUID));
  will_return (ResetSystemWithSubtype, &JumpBuf);

  if (!SetJump (&JumpBuf)) {
    Status = ConfProfileMgrDxeEntry (NULL, NULL);
    UT_ASSERT_NOT_EFI_ERROR (Status);
  }

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConfProfileMgrDxe.

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
ConfProfileMgrDxeShouldWriteGenericProfileAndReset (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS                Status;
  UINT32                    i;
  BASE_LIBRARY_JUMP_BUFFER  JumpBuf;

  // Fail to get cached variable
  will_return (MockGetVariable, sizeof (EFI_GUID));
  will_return (MockGetVariable, &gSetupDataPkgGenericProfileGuid);
  will_return (MockGetVariable, 3);
  will_return (MockGetVariable, EFI_NOT_FOUND);

  will_return (GetSectionFromAnyFv, mKnown_Good_Generic_Profile);
  will_return (GetSectionFromAnyFv, sizeof (mKnown_Good_Generic_Profile));

  // Force bad profile to be retrieved
  will_return (LibPcdGetPtr, &gZeroGuid);
  will_return (LibPcdGetPtr, &gSetupDataPkgGenericProfileGuid);

  will_return (LibPcdSetPtrS, EFI_SUCCESS);

  expect_memory (MockSetVariable, VariableName, CACHED_CONF_PROFILE_VARIABLE_NAME, StrSize (CACHED_CONF_PROFILE_VARIABLE_NAME));
  expect_memory (MockSetVariable, VendorGuid, &gConfProfileMgrVariableGuid, sizeof (EFI_GUID));
  expect_value (MockSetVariable, DataSize, sizeof (EFI_GUID));
  expect_memory (MockSetVariable, Data, &gSetupDataPkgGenericProfileGuid, sizeof (EFI_GUID));
  will_return (MockSetVariable, EFI_SUCCESS);

  // Cause profile to be validated
  will_return (IsSystemInManufacturingMode, FALSE);

  // Force profile to not match flash with bad entry
  will_return (MockGetVariable, mKnown_Good_VarList_DataSizes[0]);
  will_return (MockGetVariable, mKnown_Good_VarList_Entries[1]);

  will_return (MockGetVariable, 3);
  will_return (MockGetVariable, EFI_SUCCESS);

  // set var
  expect_memory (MockSetVariable, VariableName, mKnown_Good_VarList_Names[0], StrSize (mKnown_Good_VarList_Names[0]));
  expect_memory (MockSetVariable, VendorGuid, &mKnown_Good_Yaml_Guid, sizeof (EFI_GUID));
  expect_value (MockSetVariable, DataSize, mKnown_Good_VarList_DataSizes[0]);
  expect_memory (MockSetVariable, Data, mKnown_Good_VarList_Entries[0], mKnown_Good_VarList_DataSizes[0]);
  will_return (MockSetVariable, EFI_SUCCESS);

  for (i = 1; i < 7; i++) {
    will_return (MockGetVariable, mKnown_Good_VarList_DataSizes[i]);
    will_return (MockGetVariable, mKnown_Good_VarList_Entries[i]);

    if (i < 2) {
      will_return (MockGetVariable, 3);
    } else {
      // XML part of blob
      will_return (MockGetVariable, 7);
    }

    will_return (MockGetVariable, EFI_SUCCESS);
  }

  // Force profile to not match flash with bad datasize
  will_return (MockGetVariable, 140);

  // delete var
  expect_memory (MockSetVariable, VariableName, mKnown_Good_VarList_Names[7], StrSize (mKnown_Good_VarList_Names[7]));
  expect_memory (MockSetVariable, VendorGuid, &mKnown_Good_Xml_Guid, sizeof (EFI_GUID));
  expect_value (MockSetVariable, DataSize, 0);
  expect_value (MockSetVariable, Data, NULL);
  will_return (MockSetVariable, EFI_SUCCESS);

  // set var
  expect_memory (MockSetVariable, VariableName, mKnown_Good_VarList_Names[7], StrSize (mKnown_Good_VarList_Names[7]));
  expect_memory (MockSetVariable, VendorGuid, &mKnown_Good_Xml_Guid, sizeof (EFI_GUID));
  expect_value (MockSetVariable, DataSize, mKnown_Good_VarList_DataSizes[7]);
  expect_memory (MockSetVariable, Data, mKnown_Good_VarList_Entries[7], mKnown_Good_VarList_DataSizes[7]);
  will_return (MockSetVariable, EFI_SUCCESS);

  // Force profile to not match flash with bad attribute
  will_return (MockGetVariable, mKnown_Good_VarList_DataSizes[8]);
  will_return (MockGetVariable, mKnown_Good_VarList_Entries[8]);

  will_return (MockGetVariable, 2);
  will_return (MockGetVariable, EFI_SUCCESS);

  // delete var
  expect_memory (MockSetVariable, VariableName, mKnown_Good_VarList_Names[8], StrSize (mKnown_Good_VarList_Names[8]));
  expect_memory (MockSetVariable, VendorGuid, &mKnown_Good_Xml_Guid, sizeof (EFI_GUID));
  expect_value (MockSetVariable, DataSize, 0);
  expect_value (MockSetVariable, Data, NULL);
  will_return (MockSetVariable, EFI_SUCCESS);

  // set var
  expect_memory (MockSetVariable, VariableName, mKnown_Good_VarList_Names[8], StrSize (mKnown_Good_VarList_Names[8]));
  expect_memory (MockSetVariable, VendorGuid, &mKnown_Good_Xml_Guid, sizeof (EFI_GUID));
  expect_value (MockSetVariable, DataSize, mKnown_Good_VarList_DataSizes[8]);
  expect_memory (MockSetVariable, Data, mKnown_Good_VarList_Entries[8], mKnown_Good_VarList_DataSizes[8]);
  will_return (MockSetVariable, EFI_SUCCESS);

  expect_memory (ResetSystemWithSubtype, ResetSubtype, &gConfProfileMgrResetGuid, sizeof (EFI_GUID));
  will_return (ResetSystemWithSubtype, &JumpBuf);

  if (!SetJump (&JumpBuf)) {
    Status = ConfProfileMgrDxeEntry (NULL, NULL);
    UT_ASSERT_NOT_EFI_ERROR (Status);
  }

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConfProfileMgrDxe.

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
ConfProfileMgrDxeShouldUseRetrievedProfileMfgMode (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  // Getting cached variable, it should not write variable
  will_return (MockGetVariable, sizeof (EFI_GUID));
  will_return (MockGetVariable, &gSetupDataPkgGenericProfileGuid);
  will_return (MockGetVariable, 3);
  will_return (MockGetVariable, EFI_SUCCESS);

  will_return (LibPcdGetPtr, &gSetupDataPkgGenericProfileGuid);

  will_return (LibPcdSetPtrS, EFI_SUCCESS);

  // Cause profile to not be validated
  will_return (IsSystemInManufacturingMode, TRUE);

  will_return (MockInstallProtocolInterface, EFI_SUCCESS);

  Status = ConfProfileMgrDxeEntry (NULL, NULL);
  UT_ASSERT_NOT_EFI_ERROR (Status);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConfProfileMgrDxe.

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
ConfProfileMgrDxeShouldUseCachedProfileMfgMode (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  // Getting cached variable, it should not write variable
  will_return (MockGetVariable, sizeof (EFI_GUID));
  will_return (MockGetVariable, &gSetupDataPkgGenericProfileGuid);
  will_return (MockGetVariable, 3);
  will_return (MockGetVariable, EFI_SUCCESS);

  // Force bad profile to be retrieved
  will_return (LibPcdGetPtr, &gZeroGuid);

  will_return (LibPcdSetPtrS, EFI_SUCCESS);

  // Cause profile to not be validated
  will_return (IsSystemInManufacturingMode, TRUE);

  will_return (MockInstallProtocolInterface, EFI_SUCCESS);

  Status = ConfProfileMgrDxeEntry (NULL, NULL);
  UT_ASSERT_NOT_EFI_ERROR (Status);

  return UNIT_TEST_PASSED;
}

/**
  Unit test for ConfProfileMgrDxe.

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
ConfProfileMgrDxeShouldUseGenericProfileMfgMode (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;

  // Fail to get cached variable
  will_return (MockGetVariable, sizeof (EFI_GUID));
  will_return (MockGetVariable, &gSetupDataPkgGenericProfileGuid);
  will_return (MockGetVariable, 3);
  will_return (MockGetVariable, EFI_NOT_FOUND);

  // Force bad profile to be retrieved
  will_return (LibPcdGetPtr, &gZeroGuid);

  will_return (LibPcdSetPtrS, EFI_SUCCESS);

  expect_memory (MockSetVariable, VariableName, CACHED_CONF_PROFILE_VARIABLE_NAME, StrSize (CACHED_CONF_PROFILE_VARIABLE_NAME));
  expect_memory (MockSetVariable, VendorGuid, &gConfProfileMgrVariableGuid, sizeof (EFI_GUID));
  expect_value (MockSetVariable, DataSize, sizeof (EFI_GUID));
  expect_memory (MockSetVariable, Data, &gSetupDataPkgGenericProfileGuid, sizeof (EFI_GUID));
  will_return (MockSetVariable, EFI_SUCCESS);

  // Cause profile to not be validated
  will_return (IsSystemInManufacturingMode, TRUE);

  will_return (MockInstallProtocolInterface, EFI_SUCCESS);

  Status = ConfProfileMgrDxeEntry (NULL, NULL);
  UT_ASSERT_NOT_EFI_ERROR (Status);

  return UNIT_TEST_PASSED;
}

/**
  Initialize the unit test framework, suite, and unit tests for ConfProfileMgrDxe
  and run the unit tests.

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
  UNIT_TEST_SUITE_HANDLE      ConfProfileMgrDxeTests;
  UNIT_TEST_SUITE_HANDLE      ActiveProfileSelectorLibNullTests;

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
  // Populate the ActiveProfileSelectorLib Unit Test Suite.
  //
  Status = CreateUnitTestSuite (&ActiveProfileSelectorLibNullTests, Framework, "ActiveProfileSelectorLibNull Tests", "ActiveProfileSelectorLibNull", NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in CreateUnitTestSuite for ConfProfileMgrDxeTests\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  //
  // Populate the ConfProfileMgrDxe Unit Test Suite.
  //
  Status = CreateUnitTestSuite (&ConfProfileMgrDxeTests, Framework, "ConfProfileMgrDxe Tests", "ConfProfileMgrDxe.Conf", NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in CreateUnitTestSuite for ConfProfileMgrDxeTests\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  //
  // --------------Suite-----------Description--------------Name----------Function--------Pre---Post-------------------Context-----------
  //
  AddTestCase (ActiveProfileSelectorLibNullTests, "RetrieveActiveProfileGuid should succeed when given generic profile", "RetrieveActiveProfileGuidShouldMatch", RetrieveActiveProfileGuidShouldMatch, NULL, NULL, NULL);
  AddTestCase (ActiveProfileSelectorLibNullTests, "RetrieveActiveProfileGuid should fail when given bad profile", "RetrieveActiveProfileGuidShouldFail", RetrieveActiveProfileGuidShouldFail, NULL, NULL, NULL);

  //
  // ConfProfileMgrDxe in expected cases
  //
  AddTestCase (ConfProfileMgrDxeTests, "ConfProfileMgrDxe should use the retrieved active profile", "ConfProfileMgrDxeShouldUseRetrievedProfile", ConfProfileMgrDxeShouldUseRetrievedProfile, NULL, NULL, NULL);
  AddTestCase (ConfProfileMgrDxeTests, "ConfProfileMgrDxe should use the cached profile", "ConfProfileMgrDxeShouldUseCachedProfile", ConfProfileMgrDxeShouldUseCachedProfile, NULL, NULL, NULL);
  AddTestCase (ConfProfileMgrDxeTests, "ConfProfileMgrDxe should use the generic profile", "ConfProfileMgrDxeShouldUseGenericProfile", ConfProfileMgrDxeShouldUseGenericProfile, NULL, NULL, NULL);

  //
  // ConfProfileMgrDxe hits assert path
  //
  AddTestCase (ConfProfileMgrDxeTests, "ConfProfileMgrDxe should assert", "ConfProfileMgrDxeShouldAssert", ConfProfileMgrDxeShouldAssert, NULL, NULL, NULL);

  //
  // Profile doesn't match flash test cases
  //
  AddTestCase (ConfProfileMgrDxeTests, "ConfProfileMgrDxe should write the received active profile and reset", "ConfProfileMgrDxeShouldWriteReceivedProfileAndReset", ConfProfileMgrDxeShouldWriteReceivedProfileAndReset, NULL, NULL, NULL);
  AddTestCase (ConfProfileMgrDxeTests, "ConfProfileMgrDxe should write the cached profile and reset", "ConfProfileMgrDxeShouldWriteCachedProfileAndReset", ConfProfileMgrDxeShouldWriteCachedProfileAndReset, NULL, NULL, NULL);
  AddTestCase (ConfProfileMgrDxeTests, "ConfProfileMgrDxe should write the generic profile and reset", "ConfProfileMgrDxeShouldWriteGenericProfileAndReset", ConfProfileMgrDxeShouldWriteGenericProfileAndReset, NULL, NULL, NULL);

  //
  // ConfProfileMgrDxe in MfgMode
  //
  AddTestCase (ConfProfileMgrDxeTests, "ConfProfileMgrDxe should use the retrieved active profile in Mfg Mode", "ConfProfileMgrDxeShouldUseRetrievedProfileMfgMode", ConfProfileMgrDxeShouldUseRetrievedProfileMfgMode, NULL, NULL, NULL);
  AddTestCase (ConfProfileMgrDxeTests, "ConfProfileMgrDxe should use the cached profile in Mfg Mode", "ConfProfileMgrDxeShouldUseCachedProfileMfgMode", ConfProfileMgrDxeShouldUseCachedProfileMfgMode, NULL, NULL, NULL);
  AddTestCase (ConfProfileMgrDxeTests, "ConfProfileMgrDxe should use the generic profile in Mfg Mode", "ConfProfileMgrDxeShouldUseGenericProfileMfgMode", ConfProfileMgrDxeShouldUseGenericProfileMfgMode, NULL, NULL, NULL);

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
