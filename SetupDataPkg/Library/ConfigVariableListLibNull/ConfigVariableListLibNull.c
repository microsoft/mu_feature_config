/** @file
  Null library instance to process the list of configuration variables.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/ConfigVariableListLib.h>

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
  return EFI_UNSUPPORTED;
}

/**
  Find specified active configuration variable for this platform.

  @param[in]  VarListName       NULL terminated unicode variable name of interest.
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
  return EFI_UNSUPPORTED;
}

/**
  Find specified active configuration variable for this platform.

  @param[in]  VarListName       NULL terminated ascii variable name of interest.
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
  return EFI_UNSUPPORTED;
}
