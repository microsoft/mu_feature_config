/** @file
  Library interface to process the list of configuration variables.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <PiDxe.h>
#include <Library/DxeServicesLib.h>
#include <Library/PcdLib.h>
#include <Library/ConfigVariableListLib.h>
#include <Library/SafeIntLib.h>

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
  )
{
  UINT32         CalculatedSize = sizeof (CONFIG_VAR_LIST_HDR) + sizeof (EFI_GUID) + sizeof (UINT32) + sizeof (UINT32);
  RETURN_STATUS  Status;

  if (NeededSize == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = SafeUint32Add (CalculatedSize, NameSize, &CalculatedSize);
  if (RETURN_ERROR (Status)) {
    return (EFI_STATUS)Status;
  }

  Status = SafeUint32Add (CalculatedSize, DataSize, &CalculatedSize);
  if (RETURN_ERROR (Status)) {
    return (EFI_STATUS)Status;
  }

  *NeededSize = CalculatedSize;

  return EFI_SUCCESS;
}

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
  )
{
  CONST EFI_GUID             *Guid;
  CONST CHAR16               *NameInBin;
  CONST CHAR8                *DataInBin;
  CONST CONFIG_VAR_LIST_HDR  *VarList = NULL;
  UINTN                      BinSize  = 0;
  EFI_STATUS                 Status   = EFI_SUCCESS;
  CHAR16                     *VarName = NULL;
  CHAR8                      *Data    = NULL;
  UINT32                     Attributes;
  UINT32                     CRC32;
  UINT32                     CalcCRC32;
  UINT32                     NeededSize = 0;
  CONFIG_VAR_LIST_ENTRY      *Entry;

  // Sanity check for input parameters
  if ((VariableListBuffer == NULL) || (Size == NULL) || (VariableEntry == NULL)) {
    Status = EFI_INVALID_PARAMETER;
    goto Exit;
  }

  if (*Size < sizeof (*VarList)) {
    Status = EFI_BUFFER_TOO_SMALL;
    goto Exit;
  }

  // index into variable list
  BinSize = *Size;
  VarList = (CONST CONFIG_VAR_LIST_HDR *)((CHAR8 *)VariableListBuffer);
  Status  = GetVarListSize (VarList->NameSize, VarList->DataSize, &NeededSize);

  if (EFI_ERROR (Status)) {
    // we overflowed
    DEBUG ((
      DEBUG_ERROR,
      "%a VarList size overflowed, too large of config! NameSize: 0x%x DataSize: 0x%x\n",
      __func__,
      VarList->NameSize,
      VarList->DataSize
      ));
    goto Exit;
  }

  if ((UINTN)NeededSize > BinSize) {
    // the NameSize and DataInBinSize have bad values and are pushing us past the end of the binary
    DEBUG ((DEBUG_ERROR, "%a VarList buffer does not have needed size (actual: %x, expected: %x)\n", __func__, BinSize, NeededSize));
    *Size  = (UINTN)NeededSize;
    Status = EFI_BUFFER_TOO_SMALL;
    goto Exit;
  }

  // Use this as stub to indicate how much buffer used.
  *Size = (UINTN)NeededSize;

  /*
    * Var List is in DmpStore format:
    *
    *  struct {
    *    CONFIG_VAR_LIST_HDR VarList;
    *    CHAR16 Name[VarList->NameSize/2];
    *    EFI_GUID Guid;
    *    UINT32 Attributes;
    *    CHAR8 DataInBin[VarList-DataSize];
    *    UINT32 CRC32; // CRC32 of all bytes from VarList to end of DataInBin
    *  }
    */
  NameInBin = (CONST CHAR16 *)(VarList + 1);
  Guid      = (CONST EFI_GUID *)((CHAR8 *)NameInBin + VarList->NameSize);
  CopyMem (&Attributes, (Guid + 1), sizeof (UINT32));
  DataInBin = (CONST CHAR8 *)Guid + sizeof (*Guid) + sizeof (Attributes);
  CopyMem (&CRC32, (DataInBin + VarList->DataSize), sizeof (UINT32));

  // validate CRC32
  CalcCRC32 = CalculateCrc32 ((VOID *)VarList, NeededSize - sizeof (CRC32));
  if (CRC32 != CalcCRC32) {
    DEBUG ((DEBUG_ERROR, "%a CRC is off in the variable list: actual: %x, expect %x\n", __func__, CRC32, CalcCRC32));
    Status = EFI_COMPROMISED_DATA;
    goto Exit;
  }

  VarName = AllocatePool (VarList->NameSize);
  if (VarName == NULL) {
    DEBUG ((DEBUG_ERROR, "%a Failed to allocate memory for VarName size: %u\n", __func__, VarList->NameSize));
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  CopyMem (VarName, NameInBin, VarList->NameSize);

  Data = AllocatePool (VarList->DataSize);
  if (Data == NULL) {
    DEBUG ((DEBUG_ERROR, "%a Failed to allocate memory for Data size: %u\n", __func__, VarList->DataSize));
    Status = EFI_OUT_OF_RESOURCES;
    // Free VarName here, as other memory gets freed in exit routine
    FreePool (VarName);
    goto Exit;
  }

  CopyMem (Data, DataInBin, VarList->DataSize);

  // Add correct values to this entry in the blob
  Entry = VariableEntry;

  Entry->Name       = VarName;
  Entry->Guid       = *Guid;
  Entry->Attributes = Attributes;
  Entry->Data       = Data;
  Entry->DataSize   = VarList->DataSize;

  Status = EFI_SUCCESS;

Exit:
  return Status;
}

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
  )
{
  EFI_STATUS  Status;
  UINT32      NameSize;
  UINT32      NeededSize;
  UINTN       Offset;
  UINT32      Crc32;

  // Sanity check for input parameters
  if ((Size == NULL) || (VariableEntry == NULL) || ((VariableListBuffer == NULL) && (*Size != 0))) {
    Status = EFI_INVALID_PARAMETER;
    goto Exit;
  }

  // Sanity check 2 for input parameters
  if ((VariableEntry->Name == NULL) || (VariableEntry->Data == NULL)) {
    Status = EFI_INVALID_PARAMETER;
    goto Exit;
  }

  NameSize = (UINT32)StrnSizeS (VariableEntry->Name, CONF_VAR_NAME_LEN);
  Status   = GetVarListSize (NameSize, VariableEntry->DataSize, &NeededSize);

  if (EFI_ERROR (Status)) {
    // overflowed...
    DEBUG ((DEBUG_ERROR, "%a VarList size overflowed, too large of config!\n", __func__));
    goto Exit;
  }

  if (*Size < (UINTN)NeededSize) {
    Status = EFI_BUFFER_TOO_SMALL;
    *Size  = (UINTN)NeededSize;
    goto Exit;
  }

  Offset = 0;
  // Header
  ((CONFIG_VAR_LIST_HDR *)VariableListBuffer)->NameSize = (UINT32)NameSize;
  ((CONFIG_VAR_LIST_HDR *)VariableListBuffer)->DataSize = (UINT32)VariableEntry->DataSize;
  Offset                                               += sizeof (CONFIG_VAR_LIST_HDR);

  // Name
  CopyMem ((UINT8 *)(VariableListBuffer) + Offset, VariableEntry->Name, NameSize);
  Offset += NameSize;

  // Guid
  CopyMem ((UINT8 *)(VariableListBuffer) + Offset, &VariableEntry->Guid, sizeof (VariableEntry->Guid));
  Offset += sizeof (VariableEntry->Guid);

  // Attributes
  CopyMem ((UINT8 *)(VariableListBuffer) + Offset, &VariableEntry->Attributes, sizeof (VariableEntry->Attributes));
  Offset += sizeof (VariableEntry->Attributes);

  // Data
  CopyMem ((UINT8 *)(VariableListBuffer) + Offset, VariableEntry->Data, VariableEntry->DataSize);
  Offset += VariableEntry->DataSize;

  // CRC32
  Crc32 = CalculateCrc32 (VariableListBuffer, Offset);
  CopyMem ((UINT8 *)(VariableListBuffer) + Offset, &Crc32, sizeof (UINT32));
  Offset += sizeof (UINT32);

  *Size = (UINTN)NeededSize;

  // They should still match in size...
  if (Offset != (UINTN)NeededSize) {
    ASSERT (Offset == (UINTN)NeededSize);
    Status = EFI_COMPROMISED_DATA;
    goto Exit;
  }

  Status = EFI_SUCCESS;

Exit:
  return Status;
}

/**
  Parse Active Config Variable List and return full list or specific entry if VarName parameter != NULL

  @param[in]  VariableListBuffer      Pointer to raw variable list buffer.
  @param[in]  VariableListBufferSize  Size of VariableListBuffer.
  @param[out] ConfigVarListPtr        Pointer to configuration data. User is responsible to free the
                                      returned buffer and the Data, Name fields for each entry.
  @param[out] ConfigVarListCount      Number of variable list entries.
  @param[in]  ConfigVarName           If NULL, return full list, else return entry for that variable

  @retval EFI_INVALID_PARAMETER   Input argument is null.
  @retval EFI_OUT_OF_RESOURCES    Memory allocation failed.
  @retval EFI_NOT_FOUND           The requested variable is not found in VariableListBuffer.
  @retval EFI_COMPROMISED_DATA    The variable list buffer contains data that does not fit within the structure defined.
  @retval EFI_SUCCESS             The operation succeeds.

**/
EFI_STATUS
ParseActiveConfigVarList (
  IN  CONST VOID             *VariableListBuffer,
  IN  UINTN                  VariableListBufferSize,
  OUT CONFIG_VAR_LIST_ENTRY  **ConfigVarListPtr,
  OUT UINTN                  *ConfigVarListCount,
  IN  CONST CHAR16           *ConfigVarName
  )
{
  UINTN                      LeftSize       = 0;
  CONST CONFIG_VAR_LIST_HDR  *VarList       = NULL;
  EFI_STATUS                 Status         = EFI_SUCCESS;
  UINTN                      ListIndex      = 0;
  UINTN                      AllocatedCount = 1;

  if ((ConfigVarListPtr == NULL) || (ConfigVarListCount == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a Null parameter passed\n", __func__));
    Status = EFI_INVALID_PARAMETER;
    if (ConfigVarListCount) {
      *ConfigVarListCount = 0;
    }

    if (ConfigVarListPtr) {
      *ConfigVarListPtr = NULL;
    }

    goto Exit;
  }

  *ConfigVarListCount = 0;

  if ((VariableListBuffer == NULL) || (VariableListBufferSize == 0)) {
    DEBUG ((DEBUG_ERROR, "%a Incoming variable list buffer (base: %p, size: 0x%x) invalid\n", __func__, VariableListBuffer, VariableListBufferSize));
    *ConfigVarListPtr = NULL;
    Status            = EFI_INVALID_PARAMETER;
    goto Exit;
  }

  if (ConfigVarName == NULL) {
    // We don't know how many entries there are, for now allocate 1 entry and extend the size when needed.
    *ConfigVarListPtr = NULL;
    *ConfigVarListPtr = AllocatePool (AllocatedCount * sizeof (CONFIG_VAR_LIST_ENTRY));
  }

  if (*ConfigVarListPtr == NULL) {
    DEBUG ((DEBUG_ERROR, "%a Failed to allocate memory for ConfigVarListPtr\n", __func__));
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  while (ListIndex < VariableListBufferSize) {
    // index into variable list
    VarList = (CONST CONFIG_VAR_LIST_HDR *)((CHAR8 *)VariableListBuffer + ListIndex);

    LeftSize = VariableListBufferSize - ListIndex;
    Status   = ConvertVariableListToVariableEntry (VarList, &LeftSize, &(*ConfigVarListPtr)[*ConfigVarListCount]);

    if (EFI_ERROR (Status)) {
      // Unable to convert this specific variable list
      DEBUG ((DEBUG_ERROR, "%a Configuration VarList conversion failed %r\n", __func__, Status));
      ASSERT (FALSE);
      break;
    }

    ListIndex += LeftSize;

    // Check to see if this variable list has the target ConfigVarName
    if ((ConfigVarName != NULL) && (0 != StrnCmp (ConfigVarName, (*ConfigVarListPtr)->Name, VarList->NameSize / 2))) {
      // Not the entry we are looking for
      // While we are here, set the Name entry to NULL so we can tell if we found the entry later
      FreePool ((*ConfigVarListPtr)->Name);
      FreePool ((*ConfigVarListPtr)->Data);
      (*ConfigVarListPtr)->Name = NULL;
      (*ConfigVarListPtr)->Data = NULL;
      continue;
    }

    (*ConfigVarListCount)++;

    if (ConfigVarName != NULL) {
      // Found the entry we are looking for
      break;
    }

    // Need to reallocate if it is half full
    if (AllocatedCount <= (*ConfigVarListCount) * 2) {
      *ConfigVarListPtr = ReallocatePool (AllocatedCount * sizeof (CONFIG_VAR_LIST_ENTRY), AllocatedCount * 2 * sizeof (CONFIG_VAR_LIST_ENTRY), *ConfigVarListPtr);
      if (*ConfigVarListPtr == NULL) {
        DEBUG ((DEBUG_ERROR, "%a Failed to reallocate memory for ConfigVarListPtr count: %u\n", __func__, AllocatedCount));
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
      }

      AllocatedCount = AllocatedCount * 2;
    }
  }

  if ((*ConfigVarListPtr)->Name == NULL) {
    // We did not find the entry in the var list
    // caller is responsible for freeing input
    DEBUG ((DEBUG_ERROR, "%a Failed to find varname in var list: %s\n", __func__, ConfigVarName));
    Status = EFI_NOT_FOUND;
    goto Exit;
  }

Exit:
  if (EFI_ERROR (Status)) {
    // Need to free all allocated memory
    while (ConfigVarListCount && *ConfigVarListCount > 0) {
      if (ConfigVarListPtr && *ConfigVarListPtr && (*ConfigVarListPtr)[*ConfigVarListCount - 1].Name) {
        FreePool ((*ConfigVarListPtr)[*ConfigVarListCount - 1].Name);
      }

      if (ConfigVarListPtr && *ConfigVarListPtr && (*ConfigVarListPtr)[*ConfigVarListCount - 1].Data) {
        FreePool ((*ConfigVarListPtr)[*ConfigVarListCount - 1].Data);
      }

      (*ConfigVarListCount)--;
    }

    // only free *ConfigVarListPtr if we allocated it
    if ((ConfigVarListPtr != NULL) && (*ConfigVarListPtr != NULL) && (ConfigVarName == NULL)) {
      FreePool (*ConfigVarListPtr);
      *ConfigVarListPtr = NULL;
    }
  }

  return Status;
}

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
  )
{
  return ParseActiveConfigVarList (VariableListBuffer, VariableListBufferSize, ConfigVarListPtr, ConfigVarListCount, NULL);
}

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
  )
{
  UINTN  ConfigVarListCount = 0;

  if ((VarName == NULL) || (ConfigVarListPtr == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a Null parameter passed\n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  return ParseActiveConfigVarList (VariableListBuffer, VariableListBufferSize, &ConfigVarListPtr, &ConfigVarListCount, VarName);
}

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
  )
{
  UINTN   ConfigVarListCount = 0;
  CHAR16  *UniVarName        = NULL;
  UINTN   UniVarNameLen      = 0;

  if ((VarName == NULL) || (ConfigVarListPtr == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a Null parameter passed\n", __func__));
    return EFI_INVALID_PARAMETER;
  }

  UniVarNameLen = AsciiStrSize (VarName) * 2;

  UniVarName = AllocatePool (UniVarNameLen);
  if (UniVarName == NULL) {
    DEBUG ((DEBUG_ERROR, "%a Failed to alloc memory for UniVarName size: %u\n", __func__, UniVarNameLen));
    return EFI_OUT_OF_RESOURCES;
  }

  AsciiStrToUnicodeStrS (VarName, UniVarName, UniVarNameLen);

  return ParseActiveConfigVarList (VariableListBuffer, VariableListBufferSize, &ConfigVarListPtr, &ConfigVarListCount, UniVarName);
}
