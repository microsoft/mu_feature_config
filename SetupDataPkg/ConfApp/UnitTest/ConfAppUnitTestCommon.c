/** @file
  Unit tests of the main page of ConfApp module

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
#include <Guid/MuVarPolicyFoundationDxe.h>

#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#include <Library/UnitTestLib.h>

volatile BOOLEAN  gResetCalled = FALSE;
extern BOOLEAN    MainStateMachineRunning;

/**
  Mock version of EfiSignalEventReadyToBoot.

**/
VOID
EFIAPI
EfiSignalEventReadyToBoot (
  VOID
  )
{
  EFI_STATUS  Status = (EFI_STATUS)mock ();

  ASSERT_EFI_ERROR (Status);
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

  DEBUG ((DEBUG_INFO, "%a Name: %s, GUID: %g, Size: %x\n", __func__, VariableName, VendorGuid, *DataSize));
  check_expected (VariableName);
  check_expected (VendorGuid);
  assert_non_null (Attributes);

  *Attributes = READY_TO_BOOT_INDICATOR_VAR_ATTR;

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

  DEBUG ((DEBUG_INFO, "%a Name: %s\n", __func__, VariableName));
  DUMP_HEX (DEBUG_ERROR, 0, VendorGuid, sizeof (VendorGuid), "SetVar: ");

  check_expected (VariableName);
  check_expected (VendorGuid);
  check_expected (DataSize);
  check_expected (Data);

  return (EFI_STATUS)mock ();
}

/**
  Enumerates the current variable names.

  @param[in, out]  VariableNameSize The size of the VariableName buffer. The size must be large
                                    enough to fit input string supplied in VariableName buffer.
  @param[in, out]  VariableName     On input, supplies the last VariableName that was returned
                                    by GetNextVariableName(). On output, returns the Nullterminated
                                    string of the current variable.
  @param[in, out]  VendorGuid       On input, supplies the last VendorGuid that was returned by
                                    GetNextVariableName(). On output, returns the
                                    VendorGuid of the current variable.

  @retval EFI_SUCCESS           The function completed successfully.
  @retval EFI_NOT_FOUND         The next variable was not found.
  @retval EFI_BUFFER_TOO_SMALL  The VariableNameSize is too small for the result.
                                VariableNameSize has been updated with the size needed to complete the request.
  @retval EFI_INVALID_PARAMETER VariableNameSize is NULL.
  @retval EFI_INVALID_PARAMETER VariableName is NULL.
  @retval EFI_INVALID_PARAMETER VendorGuid is NULL.
  @retval EFI_INVALID_PARAMETER The input values of VariableName and VendorGuid are not a name and
                                GUID of an existing variable.
  @retval EFI_INVALID_PARAMETER Null-terminator is not found in the first VariableNameSize bytes of
                                the input VariableName buffer.
  @retval EFI_DEVICE_ERROR      The variable could not be retrieved due to a hardware error.

**/
EFI_STATUS
EFIAPI
MockGetNextVariableName (
  IN OUT UINTN     *VariableNameSize,
  IN OUT CHAR16    *VariableName,
  IN OUT EFI_GUID  *VendorGuid
  )
{
  UINTN     Size;
  VOID      *RetData;
  EFI_GUID  *GuidData;

  assert_non_null (VariableNameSize);
  assert_non_null (VariableName);
  assert_non_null (VendorGuid);

  Size = (UINTN)mock ();
  if (Size > *VariableNameSize) {
    // Genuinely does not fit, bail with the expected size
    *VariableNameSize = Size;
    return EFI_BUFFER_TOO_SMALL;
  } else if (Size == 0) {
    // Probably some other plans, directly return
    return (EFI_STATUS)mock ();
  }

  // Give them what they need
  *VariableNameSize = Size;
  RetData           = (VOID *)mock ();
  GuidData          = (EFI_GUID *)mock ();
  CopyMem (VendorGuid, GuidData, sizeof (EFI_GUID));
  CopyMem (VariableName, RetData, Size);

  return EFI_SUCCESS;
}

///
/// Mock version of the UEFI Runtime Services Table
///
EFI_RUNTIME_SERVICES  MockRuntime = {
  .GetVariable         = MockGetVariable,
  .SetVariable         = MockSetVariable,
  .GetNextVariableName = MockGetNextVariableName,
};

/**
  Simple helper to reset the system with a subtype GUID, and a default
  to fall back on (that isn't necessarily EfiResetPlatformSpecific).

  @param[in]  ResetType     The default EFI_RESET_TYPE of the reset.
  @param[in]  ResetSubtype  GUID pointer for the reset subtype to be used.

  @retval     Pointer to the GUIDed reset data structure. Structure is
              SIMPLE_GUIDED_RESET_DATA_SIZE in size.

**/
VOID
EFIAPI
ResetSystemWithSubtype (
  IN EFI_RESET_TYPE  ResetType,
  IN CONST  GUID     *ResetSubtype
  )
{
  gResetCalled            = TRUE;
  MainStateMachineRunning = FALSE;
  check_expected (ResetType);
  check_expected_ptr (ResetSubtype);
}
