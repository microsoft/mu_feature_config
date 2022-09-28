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

  assert_ptr_equal (VendorGuid, &gMuVarPolicyDxePhaseGuid);
  assert_memory_equal (VariableName, READY_TO_BOOT_INDICATOR_VAR_NAME, sizeof (READY_TO_BOOT_INDICATOR_VAR_NAME));
  assert_non_null (Attributes);

  *Attributes = READY_TO_BOOT_INDICATOR_VAR_ATTR;
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

  return (EFI_STATUS)mock ();
}

///
/// Mock version of the UEFI Runtime Services Table
///
EFI_RUNTIME_SERVICES  MockRuntime = {
  .GetVariable = MockGetVariable
};
