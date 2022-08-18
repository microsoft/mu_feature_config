/** @file
  Library interface to process the list of congfiguration variables.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef CONFIG_VAR_LIST_LIB_H_
#define CONFIG_VAR_LIST_LIB_H_

/*
 * Internal struct for variable list entries
 */
typedef struct {
  CHAR16                *Name;
  EFI_GUID              Guid;
  UINT32                Attributes;
  VOID                  *Data;
  UINT32                DataSize;
} CONFIG_VAR_LIST_ENTRY;

/*
 * Header for tool generated variable list entry
 */
#pragma pack(push, 1)
typedef struct {
  /* Size of Name in bytes */
  UINT32    NameSize;

  /* Size of Data in bytes */
  UINT32    DataSize;

  /*
   * Rest of Variable List struct:
   *
   * CHAR16 Name[NameSize/2] // Null terminate UTF-16LE encoded name
   * EFI_GUID Guid // namespace Guid
   * UINT32 Attributes // UEFI attributes
   * CHAR8 Data[DataSize] // actual variable value
   * UINT32 CRC32 // checksum of all bytes up to CRC32
   */
} CONFIG_VAR_LIST_HDR;
#pragma pack(pop)

/**
  Find all active configuration variables for this platform.

  @param[out] ConfigVarListPtr    Pointer to configuration data. User is responsible to free the
                                  returned buffer and the Data, Name fields for each entry.
  @param[out] ConfigVarListCount  Number of variable list entries.

  @retval EFI_INVALID_PARAMETER   Input argument is null.
  @retval EFI_SUCCESS             The operation succeeds.

**/
EFI_STATUS
EFIAPI
RetrieveActiveConfigVarList (
  OUT CONFIG_VAR_LIST_ENTRY  **ConfigVarListPtr,
  OUT UINTN                  *ConfigVarListCount
  );

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
  IN  CONST CHAR16           *VarName,
  OUT CONFIG_VAR_LIST_ENTRY  *ConfigVarListPtr
  );

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
  IN  CONST CHAR8            *VarName,
  OUT CONFIG_VAR_LIST_ENTRY  *ConfigVarListPtr
  );

#endif // CONFIG_VAR_LIST_LIB_H_
