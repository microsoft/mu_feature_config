/** @file
  Null library instance to process the list of congfiguration variables.

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
#include <Library/MemoryAllocationLib.h>
#include <Library/ConfigVariableListLib.h>
#include <Library/UnitTestLib.h>
#include <DfciSystemSettingTypes.h>

/**
  Find all active configuration variables for this platform.

  @param[out] ConfigVarListPtr    Pointer to configuration data. User is responsible to free the data.
  @param[out] ConfigVarListCount  Number of variable list entries.

  @retval EFI_INVALID_PARAMETER   Input argument is null.
  @retval EFI_SUCCESS             The operation succeeds.

**/
EFI_STATUS
EFIAPI
RetrieveActiveConfigVarList (
  OUT CONFIG_VAR_LIST_ENTRY  **ConfigVarListPtr,
  OUT UINTN                  *ConfigVarListCount
  )
{
  assert_non_null (ConfigVarListPtr);
  assert_non_null (ConfigVarListCount);

  *ConfigVarListPtr   = (CONFIG_VAR_LIST_ENTRY *)mock ();
  *ConfigVarListCount = (UINTN)mock ();
  return (EFI_STATUS)mock ();
}

/**
  Find specified active configuration variable for this platform.

  @param[in]  VarListName       NULL terminated unicode varible name of interest.
  @param[out] ConfigVarListPtr  Pointer to hold variable list entry from active profile.

  @retval EFI_INVALID_PARAMETER   Input argument is null.
  @retval EFI_UNSUPPORTED         This request is not supported on this platform.
  @retval EFI_NOT_FOUND           The requested variable is not found in the active profile.
  @retval EFI_SUCCESS             The operation succeeds.

**/
EFI_STATUS
EFIAPI
QuerySingleActiveConfigUnicodeVarList (
  IN  CONST CHAR16           *VarListName,
  OUT CONFIG_VAR_LIST_ENTRY  *ConfigVarListPtr
  )
{
  CONFIG_VAR_LIST_ENTRY  *Temp;

  assert_non_null (ConfigVarListPtr);
  check_expected (VarListName);

  Temp = (CONFIG_VAR_LIST_ENTRY *)mock ();
  CopyMem (ConfigVarListPtr, Temp, sizeof (CONFIG_VAR_LIST_ENTRY));
  return (EFI_STATUS)mock ();
}

#include <Library/DebugLib.h>

/**
  Find specified active configuration variable for this platform.

  @param[in]  VarListName       NULL terminated ascii varible name of interest.
  @param[out] ConfigVarListPtr  Pointer to hold variable list entry from active profile.

  @retval EFI_INVALID_PARAMETER   Input argument is null.
  @retval EFI_UNSUPPORTED         This request is not supported on this platform.
  @retval EFI_NOT_FOUND           The requested variable is not found in the active profile.
  @retval EFI_SUCCESS             The operation succeeds.

**/
EFI_STATUS
EFIAPI
QuerySingleActiveConfigAsciiVarList (
  IN  CONST CHAR8            *VarListName,
  OUT CONFIG_VAR_LIST_ENTRY  *ConfigVarListPtr
  )
{
  CONFIG_VAR_LIST_ENTRY  *Temp;

  assert_non_null (ConfigVarListPtr);
  check_expected (VarListName);

  Temp = (CONFIG_VAR_LIST_ENTRY *)mock ();
  if (Temp != NULL) {
    CopyMem (ConfigVarListPtr, Temp, sizeof (CONFIG_VAR_LIST_ENTRY));
    ConfigVarListPtr->Name = AllocateCopyPool ((StrnLenS (Temp->Name, DFCI_MAX_ID_LEN) + 1) * sizeof (CHAR16), Temp->Name);
    ConfigVarListPtr->Data = AllocateCopyPool (Temp->DataSize, Temp->Data);
  }

  return (EFI_STATUS)mock ();
}
