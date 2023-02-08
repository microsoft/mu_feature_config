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

EFI_STATUS
EFIAPI
ConvertVariableListToVariableEntry (
  IN      CONST VOID            *VariableListBuffer,
  IN  OUT UINTN                 *Size,
      OUT CONFIG_VAR_LIST_ENTRY *VariableEntry
  )
{
  UINTN                  BinSize  = 0;
  CONST CONFIG_VAR_LIST_HDR   *VarList = NULL;
  EFI_STATUS             Status   = EFI_SUCCESS;
  CHAR16                 *VarName = NULL;
  CHAR8                  *Data    = NULL;
  CONST CHAR16           *NameInBin;
  CONST EFI_GUID         *Guid;
  UINT32                 Attributes;
  CONST CHAR8            *DataInBin;
  UINT32                 CRC32;
  UINT32                 CalcCRC32;
  UINT32                 NeededSize = 0;
  CONFIG_VAR_LIST_ENTRY  *Entry;

  // Sanity check for input parameters
  if (VariableListBuffer == NULL || Size == NULL || VariableEntry == NULL) {
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
  NeededSize = sizeof (*VarList) + VarList->NameSize + VarList->DataSize + sizeof (*Guid) +
      sizeof (Attributes) + sizeof (CRC32);

  if (NeededSize > BinSize) {
    // the NameSize and DataInBinSize have bad values and are pushing us past the end of the binary
    DEBUG ((DEBUG_ERROR, "%a VarList buffer does not have needed size (actual: %x, expected: %x)\n", __FUNCTION__, BinSize, NeededSize));
    *Size   = NeededSize;
    Status  = EFI_BUFFER_TOO_SMALL;
    goto Exit;
  }

  // Use this as stub to indicate how much buffer used.
  *Size = NeededSize;

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
  NameInBin  = (CONST CHAR16 *)(VarList + 1);
  Guid       = (CONST EFI_GUID *)((CHAR8 *)NameInBin + VarList->NameSize);
  Attributes = *(CONST UINT32 *)(Guid + 1);
  DataInBin  = (CONST CHAR8 *)Guid + sizeof (*Guid) + sizeof (Attributes);
  CRC32      = *(CONST UINT32 *)(DataInBin + VarList->DataSize);

  // validate CRC32
  CalcCRC32 = CalculateCrc32 ((VOID*)VarList, NeededSize - sizeof (CRC32));
  if (CRC32 != CalcCRC32) {
    DEBUG ((DEBUG_ERROR, "%a CRC is off in the variable list: actual: %x, expect %x\n", __FUNCTION__, CRC32, CalcCRC32));
    Status = EFI_COMPROMISED_DATA;
    goto Exit;
  }

  VarName = AllocatePool (VarList->NameSize);
  if (VarName == NULL) {
    DEBUG ((DEBUG_ERROR, "%a Failed to allocate memory for VarName size: %u\n", __FUNCTION__, VarList->NameSize));
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  CopyMem (VarName, NameInBin, VarList->NameSize);

  Data = AllocatePool (VarList->DataSize);
  if (Data == NULL) {
    DEBUG ((DEBUG_ERROR, "%a Failed to allocate memory for Data size: %u\n", __FUNCTION__, VarList->DataSize));
    Status = EFI_OUT_OF_RESOURCES;
    // Free VarName here, as other memory gets freed in exit routine, but we do not increment
    // *ConfigVarListCount for this entry
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

EFI_STATUS
EFIAPI
ConvertVariableEntryToVariableList (
  IN      CONFIG_VAR_LIST_ENTRY *VariableEntry,
      OUT VOID                  **VariableListBuffer,
      OUT UINTN                 *Size
  )
{
  EFI_STATUS             Status;
  VOID                   *Data          = NULL;
  UINTN                  NameSize;
  UINTN                  NeededSize;
  UINTN                  Offset;

  // Sanity check for input parameters
  if (VariableListBuffer == NULL || Size == NULL || VariableEntry == NULL) {
    Status = EFI_INVALID_PARAMETER;
    goto Exit;
  }

  // Sanity check 2 for input parameters
  if (VariableEntry->Name == NULL || VariableEntry->Data == NULL) {
    Status = EFI_INVALID_PARAMETER;
    goto Exit;
  }

  NameSize = StrnSizeS (VariableEntry->Name, CONF_VAR_NAME_LEN);

  NeededSize = sizeof (CONFIG_VAR_LIST_HDR) + NameSize + VariableEntry->DataSize + sizeof (VariableEntry->Guid) +
                sizeof (VariableEntry->Attributes) + sizeof (UINT32);

  Data = AllocatePool (NeededSize);
  if (Data == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  Offset = 0;
  // Header
  ((CONFIG_VAR_LIST_HDR *)Data)->NameSize = (UINT32)NameSize;
  ((CONFIG_VAR_LIST_HDR *)Data)->DataSize = (UINT32)VariableEntry->DataSize;
  Offset                                 += sizeof (CONFIG_VAR_LIST_HDR);

  // Name
  CopyMem ((UINT8 *)Data + Offset, VariableEntry->Name, NameSize);
  Offset += NameSize;

  // Guid
  CopyMem ((UINT8 *)Data + Offset, &VariableEntry->Guid, sizeof (VariableEntry->Guid));
  Offset += sizeof (VariableEntry->Guid);

  // Attributes
  CopyMem ((UINT8 *)Data + Offset, &VariableEntry->Attributes, sizeof (VariableEntry->Attributes));
  Offset += sizeof (VariableEntry->Attributes);

  // Data
  CopyMem ((UINT8 *)Data + Offset, VariableEntry->Data, VariableEntry->DataSize);
  Offset += VariableEntry->DataSize;

  // CRC32
  *(UINT32 *)((UINT8 *)Data + Offset) = CalculateCrc32 (Data, Offset);
  Offset                             += sizeof (UINT32);

  *VariableListBuffer  = Data;
  *Size               = NeededSize;

  // They should still match in size...
  ASSERT (Offset == NeededSize);

Exit:
  return Status;
}

/**
  Parse Active Config Variable List and return full list or specific entry if VarName parameter != NULL

  @param[out] VariableListBuffer  Pointer to raw variable list buffer.
  @param[out] ConfigVarListPtr    Pointer to configuration data. User is responsible to free the
                                  returned buffer and the Data, Name fields for each entry.
  @param[out] ConfigVarListCount  Number of variable list entries.
  @param[in]  ConfigVarName       If NULL, return full list, else return entry for that variable

  @retval EFI_INVALID_PARAMETER   Input argument is null.
  @retval EFI_OUT_OF_RESOURCES    Memory allocation failed.
  @retval EFI_NOT_FOUND           The requested variable is not found in the active profile.
  @retval EFI_COMPROMISED_DATA    The profile contains data that does not fit within the structure defined.
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
  UINTN                       LeftSize = 0;
  CONST CONFIG_VAR_LIST_HDR   *VarList = NULL;
  EFI_STATUS             Status   = EFI_SUCCESS;
  UINTN                  ListIndex = 0;
  UINTN                  AllocatedCount = 1;

  if ((ConfigVarListPtr == NULL) || (ConfigVarListCount == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a Null parameter passed\n", __FUNCTION__));
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

  if (EFI_ERROR (Status) || (VariableListBuffer == NULL) || (VariableListBufferSize == 0)) {
    DEBUG ((DEBUG_ERROR, "%a Failed to read active profile data (%r)\n", __FUNCTION__, Status));
    ASSERT (FALSE);
    Status = EFI_NOT_FOUND;
    goto Exit;
  }

  if (ConfigVarName == NULL) {
    // We don't know how many entries there are, for now allocate far too large of a pool, we'll realloc once we
    // know how many entries there are. We are returning full list of entries.
    *ConfigVarListPtr = NULL;
    *ConfigVarListPtr = AllocatePool (AllocatedCount * sizeof (CONFIG_VAR_LIST_ENTRY));
  }

  if (*ConfigVarListPtr == NULL) {
    DEBUG ((DEBUG_ERROR, "%a Failed to allocate memory for ConfigVarListPtr\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  while (ListIndex < VariableListBufferSize) {
    // index into variable list
    VarList = (CONST CONFIG_VAR_LIST_HDR *)((CHAR8 *)VariableListBuffer + ListIndex);

    LeftSize = VariableListBufferSize - ListIndex;
    Status = ConvertVariableListToVariableEntry (VarList, &LeftSize, &(*ConfigVarListPtr)[*ConfigVarListCount]);

    if (EFI_ERROR (Status)) {
      // the NameSize and DataInBinSize have bad values and are pushing us past the end of the binary
      DEBUG ((DEBUG_ERROR, "%a Configuration VarList conversion failed %r\n", __FUNCTION__, Status));
      ASSERT (FALSE);
      break;
    }

    ListIndex += LeftSize;

    // This check needs to come after we allocate VarName so we can compare against a 16 bit aligned
    // string and not assert
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
        DEBUG ((DEBUG_ERROR, "%a Failed to reallocate memory for ConfigVarListPtr count: %u\n", __FUNCTION__, AllocatedCount));
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
      }

      AllocatedCount = AllocatedCount * 2;
    }
  }

  if ((*ConfigVarListPtr)->Name == NULL) {
    // We did not find the entry in the var list
    // caller is responsible for freeing input
    DEBUG ((DEBUG_ERROR, "%a Failed to find varname in var list: %s\n", __FUNCTION__, ConfigVarName));
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

  @param[out] ConfigVarListPtr    Pointer to configuration data. User is responsible to free the
                                  returned buffer and the Data, Name fields for each entry.
  @param[out] ConfigVarListCount  Number of variable list entries.

  @retval EFI_INVALID_PARAMETER   Input argument is null.
  @retval EFI_OUT_OF_RESOURCES    Memory allocation failed.
  @retval EFI_NOT_FOUND           The requested variable is not found in the active profile.
  @retval EFI_COMPROMISED_DATA    The profile contains data that does not fit within the structure defined.
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

  @param[in]  VarListName       NULL terminated unicode variable name of interest.
  @param[out] ConfigVarListPtr  Pointer to hold variable list entry from active profile.

  @retval EFI_UNSUPPORTED         Unsupported operation on this platform.
  @retval EFI_INVALID_PARAMETER   Input argument is null.
  @retval EFI_OUT_OF_RESOURCES    Memory allocation failed.
  @retval EFI_NOT_FOUND           The requested variable is not found in the active profile.
  @retval EFI_COMPROMISED_DATA    The profile contains data that does not fit within the structure defined.
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
    DEBUG ((DEBUG_ERROR, "%a Null parameter passed\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  return ParseActiveConfigVarList (VariableListBuffer, VariableListBufferSize, &ConfigVarListPtr, &ConfigVarListCount, VarName);
}

/**
  Find specified active configuration variable for this platform.

  @param[in]  VarListName       NULL terminated ascii variable name of interest.
  @param[out] ConfigVarListPtr  Pointer to hold variable list entry from active profile.

  @retval EFI_UNSUPPORTED         Unsupported operation on this platform.
  @retval EFI_INVALID_PARAMETER   Input argument is null.
  @retval EFI_OUT_OF_RESOURCES    Memory allocation failed.
  @retval EFI_NOT_FOUND           The requested variable is not found in the active profile.
  @retval EFI_COMPROMISED_DATA    The profile contains data that does not fit within the structure defined.
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
    DEBUG ((DEBUG_ERROR, "%a Null parameter passed\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  UniVarNameLen = AsciiStrSize (VarName) * 2;

  UniVarName = AllocatePool (UniVarNameLen);
  if (UniVarName == NULL) {
    DEBUG ((DEBUG_ERROR, "%a Failed to alloc memory for UniVarName size: %u\n", __FUNCTION__, UniVarNameLen));
    return EFI_OUT_OF_RESOURCES;
  }

  AsciiStrToUnicodeStrS (VarName, UniVarName, UniVarNameLen);

  return ParseActiveConfigVarList (VariableListBuffer, VariableListBufferSize, &ConfigVarListPtr, &ConfigVarListCount, UniVarName);
}
