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
#include <Library/ConfigVariableListLib.h>

/**
  Parse Active Config Variable List and return full list or specific entry if VarName parameter != NULL

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
  OUT CONFIG_VAR_LIST_ENTRY  **ConfigVarListPtr,
  OUT UINTN                  *ConfigVarListCount,
  IN  CONST CHAR16           *ConfigVarName
  )
{
  UINTN                  BinSize  = 0;
  CONFIG_VAR_LIST_HDR    *VarList = NULL;
  EFI_STATUS             Status   = EFI_SUCCESS;
  CHAR16                 *VarName = NULL;
  CHAR8                  *Data    = NULL;
  CHAR16                 *NameInBin;
  EFI_GUID               *Guid;
  UINT32                 Attributes;
  CHAR8                  *DataInBin;
  UINT32                 CRC32;
  UINT32                 ListIndex = 0;
  CONFIG_VAR_LIST_ENTRY  *Entry;
  VOID                   *BlobPtr = NULL;

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

  // Populate the slot with default one from FV.
  Status = GetSectionFromAnyFv (
             (EFI_GUID *)PcdGetPtr (PcdSetupConfigActiveProfileFile),
             EFI_SECTION_RAW,
             0,
             &BlobPtr,
             &BinSize
             );

  if (EFI_ERROR (Status) || (BlobPtr == NULL) || (BinSize == 0)) {
    DEBUG ((DEBUG_ERROR, "%a Failed to read active profile data (%r)\n", __FUNCTION__, Status));
    ASSERT (FALSE);
    Status = EFI_NOT_FOUND;
    goto Exit;
  }

  if (ConfigVarName == NULL) {
    // We don't know how many entries there are, for now allocate far too large of a pool, we'll realloc once we
    // know how many entries there are. We are returning full list of entries.
    *ConfigVarListPtr = NULL;
    *ConfigVarListPtr = AllocatePool (BinSize);
  }

  if (*ConfigVarListPtr == NULL) {
    DEBUG ((DEBUG_ERROR, "%a Failed to allocate memory for ConfigVarListPtr\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  while (ListIndex < BinSize) {
    // index into variable list
    VarList = (CONFIG_VAR_LIST_HDR *)((CHAR8 *)BlobPtr + ListIndex);

    if (ListIndex + sizeof (*VarList) + VarList->NameSize + VarList->DataSize + sizeof (*Guid) +
        sizeof (Attributes) + sizeof (CRC32) > BinSize)
    {
      // the NameSize and DataInBinSize have bad values and are pushing us past the end of the binary
      DEBUG ((DEBUG_ERROR, "%a Configuration VarList had bad NameSize or DataInBinSize\n", __FUNCTION__));
      ASSERT (FALSE);
      Status = EFI_COMPROMISED_DATA;
      break;
    }

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
    NameInBin  = (CHAR16 *)(VarList + 1);
    Guid       = (EFI_GUID *)((CHAR8 *)NameInBin + VarList->NameSize);
    Attributes = *(UINT32 *)(Guid + 1);
    DataInBin  = (CHAR8 *)Guid + sizeof (*Guid) + sizeof (Attributes);
    CRC32      = *(UINT32 *)(DataInBin + VarList->DataSize);

    // on next iteration, skip past this variable
    ListIndex += sizeof (*VarList) + VarList->NameSize + VarList->DataSize + sizeof (*Guid) + sizeof (Attributes)
                 + sizeof (CRC32);

    VarName = AllocatePool (VarList->NameSize);
    if (VarName == NULL) {
      DEBUG ((DEBUG_ERROR, "%a Failed to allocate memory for VarName size: %u\n", __FUNCTION__, VarList->NameSize));
      Status = EFI_OUT_OF_RESOURCES;
      goto Exit;
    }

    CopyMem (VarName, NameInBin, VarList->NameSize);

    // This check needs to come after we allocate VarName so we can compare against a 16 bit aligned
    // string and not assert
    if ((ConfigVarName != NULL) && (0 != StrnCmp (ConfigVarName, VarName, VarList->NameSize / 2))) {
      // Not the entry we are looking for
      // While we are here, set the Name entry to NULL so we can tell if we found the entry later
      (*ConfigVarListPtr)->Name = NULL;
      FreePool (VarName);
      continue;
    }

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
    Entry = &(*ConfigVarListPtr)[*ConfigVarListCount];

    Entry->Name       = VarName;
    Entry->Guid       = *Guid;
    Entry->Attributes = Attributes;
    Entry->Data       = Data;
    Entry->DataSize   = VarList->DataSize;
    (*ConfigVarListCount)++;

    if (ConfigVarName != NULL) {
      // Found the entry we are looking for
      break;
    }
  }

  if (ConfigVarName == NULL) {
    // No need to realloc for single entry
    *ConfigVarListPtr = ReallocatePool (BinSize, sizeof (CONFIG_VAR_LIST_ENTRY) * *ConfigVarListCount, *ConfigVarListPtr);
    if (*ConfigVarListPtr == NULL) {
      DEBUG ((DEBUG_ERROR, "%a Failed to reallocate memory for ConfigVarListPtr size: %u\n", __FUNCTION__, BinSize));
      Status = EFI_OUT_OF_RESOURCES;
      goto Exit;
    }
  } else if ((*ConfigVarListPtr)->Name == NULL) {
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
  OUT CONFIG_VAR_LIST_ENTRY  **ConfigVarListPtr,
  OUT UINTN                  *ConfigVarListCount
  )
{
  return ParseActiveConfigVarList (ConfigVarListPtr, ConfigVarListCount, NULL);
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
  IN  CONST CHAR16           *VarName,
  OUT CONFIG_VAR_LIST_ENTRY  *ConfigVarListPtr
  )
{
  UINTN  ConfigVarListCount = 0;

  if ((VarName == NULL) || (ConfigVarListPtr == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a Null parameter passed\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  return ParseActiveConfigVarList (&ConfigVarListPtr, &ConfigVarListCount, VarName);
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

  return ParseActiveConfigVarList (&ConfigVarListPtr, &ConfigVarListCount, UniVarName);
}
