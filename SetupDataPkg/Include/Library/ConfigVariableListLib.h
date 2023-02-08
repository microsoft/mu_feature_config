/** @file
  Library interface to process the list of configuration variables.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef CONFIG_VAR_LIST_LIB_H_
#define CONFIG_VAR_LIST_LIB_H_

// Maximum variable length accepted for this library, in bytes.
#define CONF_VAR_NAME_LEN  0x80

/*
 * Internal struct for variable list entries
 */
typedef struct {
  CHAR16      *Name;
  EFI_GUID    Guid;
  UINT32      Attributes;
  VOID        *Data;
  UINT32      DataSize;
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
  Return the size of the variable list given a NameSize (including null terminator) and DataSize

  @param[in]  NameSize    Size in bytes of the CHAR16 name of the config knob including null terminator
  @param[in]  DataSize    Size in bytes of the Data of the config knob
  @param[out] NeededSize  Size in bytes of the variable list based on these inputs. Unchanged if not EFI_SUCCESS returned.

  @retval EFI_INVALID_PARAMETER NeededSize was null
  @retval EFI_BUFFER_TOO_SMALL  Overflow occurred on addition
  @retval EFI_SUCCESS           NeededSize contains the variable list size
**/
EFI_STATUS
EFIAPI
GetVarListSize (
  IN  UINT32  NameSize,
  IN  UINT32  DataSize,
  OUT UINT32  *NeededSize
  );

/**
  Find all active configuration variables for this platform.

  @param[in]  VariableListBuffer      Pointer to raw variable list buffer.
  @param[in]  VariableListBufferSize  Size of VariableListBuffer.
  @param[out] ConfigVarListPtr        Pointer to configuration data. User is responsible to free the
                                      returned buffer and the Data, Name fields for each entry.
  @param[out] ConfigVarListCount      Number of variable list entries.

  @retval EFI_INVALID_PARAMETER   Input argument is null.
  @retval EFI_OUT_OF_RESOURCES    Memory allocation failed.
  @retval EFI_NOT_FOUND           The requested variable is not found in VariableListBuffer.
  @retval EFI_COMPROMISED_DATA    The variable list buffer contains data that does not fit within the structure defined.
  @retval EFI_SUCCESS             The operation succeeds.

**/
EFI_STATUS
EFIAPI
RetrieveActiveConfigVarList (
  IN  CONST VOID             *VariableListBuffer,
  IN  UINTN                  VariableListBufferSize,
  OUT CONFIG_VAR_LIST_ENTRY  **ConfigVarListPtr,
  OUT UINTN                  *ConfigVarListCount
  );

/**
  Find specified active configuration variable for this platform.

  @param[in]  VariableListBuffer      Pointer to raw variable list buffer.
  @param[in]  VariableListBufferSize  Size of VariableListBuffer.
  @param[in]  VarListName             NULL terminated unicode variable name of interest.
  @param[out] ConfigVarListPtr        Pointer to hold variable list entry from VariableListBuffer.

  @retval EFI_UNSUPPORTED         Unsupported operation on this platform.
  @retval EFI_INVALID_PARAMETER   Input argument is null.
  @retval EFI_OUT_OF_RESOURCES    Memory allocation failed.
  @retval EFI_NOT_FOUND           The requested variable is not found in VariableListBuffer.
  @retval EFI_COMPROMISED_DATA    The variable list buffer contains data that does not fit within the structure defined.
  @retval EFI_SUCCESS             The operation succeeds.

**/
EFI_STATUS
EFIAPI
QuerySingleActiveConfigUnicodeVarList (
  IN  VOID                   *VariableListBuffer,
  IN  UINTN                  VariableListBufferSize,
  IN  CONST CHAR16           *VarName,
  OUT CONFIG_VAR_LIST_ENTRY  *ConfigVarListPtr
  );

/**
  Find specified active configuration variable for this platform.

  @param[in]  VariableListBuffer      Pointer to raw variable list buffer.
  @param[in]  VariableListBufferSize  Size of VariableListBuffer.
  @param[in]  VarListName             NULL terminated ascii variable name of interest.
  @param[out] ConfigVarListPtr        Pointer to hold variable list entry from VariableListBuffer.

  @retval EFI_UNSUPPORTED         Unsupported operation on this platform.
  @retval EFI_INVALID_PARAMETER   Input argument is null.
  @retval EFI_OUT_OF_RESOURCES    Memory allocation failed.
  @retval EFI_NOT_FOUND           The requested variable is not found in VariableListBuffer.
  @retval EFI_COMPROMISED_DATA    The variable list buffer contains data that does not fit within the structure defined.
  @retval EFI_SUCCESS             The operation succeeds.

**/
EFI_STATUS
EFIAPI
QuerySingleActiveConfigAsciiVarList (
  IN  VOID                   *VariableListBuffer,
  IN  UINTN                  VariableListBufferSize,
  IN  CONST CHAR8            *VarName,
  OUT CONFIG_VAR_LIST_ENTRY  *ConfigVarListPtr
  );

/**
  Helper function to convert variable list to variable entry.

  @param[in]      VariableListBuffer    Pointer to buffer containing target variable list.
  @param[in,out]  Size                  On input, it indicates the size of input buffer. On output,
                                        it indicates the buffer consumed after converting to
                                        VariableEntry.
  @param[out]     VariableEntry         Pointer to converted variable entry. Upon successful return,
                                        callers are responsible for freeing the Name and Data fields.

  @retval EFI_INVALID_PARAMETER   One or more input arguments are null.
  @retval EFI_OUT_OF_RESOURCES    Memory allocation failed.
  @retval EFI_BUFFER_TOO_SMALL    The input buffer does not contain a full variable list.
  @retval EFI_COMPROMISED_DATA    The input variable list buffer has a corrupted CRC.
  @retval EFI_SUCCESS             The operation succeeds.

**/
EFI_STATUS
EFIAPI
ConvertVariableListToVariableEntry (
  IN      CONST VOID         *VariableListBuffer,
  IN  OUT UINTN              *Size,
  OUT CONFIG_VAR_LIST_ENTRY  *VariableEntry
  );

/**
  Helper function to convert variable entry to variable list.

  @param[in]      VariableEntry         Pointer to variable entry to be converted.
  @param[out]     VariableListBuffer    Pointer to buffer holding returned variable list.
  @param[in,out]  Size                  On input, it indicates the size of input buffer. On output,
                                        it indicates the buffer needed for converting from
                                        VariableEntry. Updated on successful conversion and EFI_BUFFER_TOO_SMALL
                                        returns.

  @retval EFI_INVALID_PARAMETER   One or more input arguments are null.
  @retval EFI_OUT_OF_RESOURCES    Memory allocation failed.
  @retval EFI_BUFFER_TOO_SMALL    The input buffer does not contain a full variable list.
  @retval EFI_COMPROMISED_DATA    The input variable list buffer has a corrupted CRC.
  @retval EFI_SUCCESS             The operation succeeds.

**/
EFI_STATUS
EFIAPI
ConvertVariableEntryToVariableList (
  IN  CONST CONFIG_VAR_LIST_ENTRY  *VariableEntry,
  OUT VOID                         *VariableListBuffer,
  IN OUT UINTN                     *Size
  );

#endif // CONFIG_VAR_LIST_LIB_H_
