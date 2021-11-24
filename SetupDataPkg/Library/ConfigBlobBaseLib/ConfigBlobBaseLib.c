/** @file

Copyright (c) 2017 - 2020, Intel Corporation. All rights reserved.<BR>
Copyright (c) Microsoft Corporation.
SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/ConfigDataLib.h>
#include <Library/BaseMemoryLib.h>

/**
  Find configuration data header by its tag and platform ID.

  @param[in] ConfBlob    Pointer to configuration data blob to be looked at.
  @param[in] PidMask     Platform ID mask.
  @param[in] Tag         Configuration TAG ID to find.
  @param[in] IsInternal  Search for internal data base only if it is non-zero.
  @param[in] Level       Nested call level.

  @retval             Configuration data header pointer.
                      NULL if the tag cannot be found.

**/
CDATA_HEADER *
FindConfigHdrByPidMaskTag (
  VOID    *ConfBlob,
  UINT32  PidMask,
  UINT32  Tag,
  UINT8   IsInternal,
  UINT32  Level
  )
{
  CDATA_BLOB          *CdataBlob;
  CDATA_HEADER        *CdataHdr;
  UINT8                Idx;
  REFERENCE_CFG_DATA  *Refer;
  UINT32               Offset;

  CdataBlob = (CDATA_BLOB *) ConfBlob;
  if (ConfBlob == NULL || CdataBlob->Signature != CFG_DATA_SIGNATURE) {
    return NULL;
  }

  Offset    = IsInternal > 0 ? (CdataBlob->ExtraInfo.InternalDataOffset * 4) : CdataBlob->HeaderLength;

  while (Offset < CdataBlob->UsedLength) {
    CdataHdr = (CDATA_HEADER *) ((UINT8 *)CdataBlob + Offset);
    if (CdataHdr->Tag == Tag) {
      for (Idx = 0; Idx < CdataHdr->ConditionNum; Idx++) {
        // if ((PidMask & CdataHdr->Condition[Idx].Value) != 0) {
          // Found a match
          if ((CdataHdr->Flags & CDATA_FLAG_TYPE_MASK) == CDATA_FLAG_TYPE_REFER) {
            if (Level > 0) {
              // Prevent multiple level nesting
              return NULL;
            } else {
              Refer = (REFERENCE_CFG_DATA *) ((UINT8 *)CdataHdr + sizeof (CDATA_HEADER) + sizeof (
                                                CDATA_COND) * CdataHdr->ConditionNum);
              return FindConfigHdrByPidMaskTag (ConfBlob, PID_TO_MASK (Refer->PlatformId), \
                                                Refer->Tag, (UINT8)Refer->IsInternal, 1);
            }
          } else {
            return (VOID *)CdataHdr;
          }
        // }
      }
    }
    Offset += (CdataHdr->Length << 2);
  }
  return NULL;
}

/**
  Find configuration data header by its tag.

  @param[in] ConfBlob    Pointer to configuration data blob to be looked at.
  @param[in] Tag         Configuration TAG ID to find.

  @retval            Configuration data header pointer.
                     NULL if the tag cannot be found.

**/
CDATA_HEADER *
EFIAPI
FindConfigHdrByTag (
  VOID    *ConfBlob,
  UINT32  Tag
  )
{
  UINT32               PidMask;

  PidMask = MAX_UINT32; // Enforce true for simplicity PID_TO_MASK (GetPlatformId ());
  return FindConfigHdrByPidMaskTag (ConfBlob, PidMask, Tag, 0, 0);
}

/**
  Find configuration data by its tag and PlatforID.

  @param[in] ConfBlob       Pointer to configuration data blob to be looked at.
  @param[in] PlatformId     Platform ID.
  @param[in] Tag            Configuration TAG ID to find.

  @retval            Configuration data pointer.
                     NULL if the tag cannot be found.

**/
VOID *
EFIAPI
FindConfigDataByPidTag (
  VOID    *ConfBlob,
  UINT16  PlatformId,
  UINT32  Tag
  )
{
  CDATA_HEADER        *CdataHdr;
  VOID                *Cdata;

  CdataHdr = FindConfigHdrByPidMaskTag (ConfBlob, PID_TO_MASK (PlatformId), Tag, 0, 0);
  if (CdataHdr == NULL) {
    Cdata = NULL;
  } else {
    Cdata = (VOID *) ((UINT8 *)CdataHdr + sizeof (CDATA_HEADER) + sizeof (CDATA_COND) * CdataHdr->ConditionNum);
  }

  return Cdata;
}

/**
  Find configuration data by its tag.

  @param[in] ConfBlob   Pointer to configuration data blob to be looked at.
  @param[in] Tag        Configuration TAG ID to find.

  @retval            Configuration data pointer.
                     NULL if the tag cannot be found.

**/
VOID *
EFIAPI
FindConfigDataByTag (
  VOID    *ConfBlob,
  UINT32  Tag
  )
{
  CDATA_HEADER        *CdataHdr;
  VOID                *Cdata;

  CdataHdr = FindConfigHdrByTag (ConfBlob, Tag);
  if (CdataHdr == NULL) {
    Cdata = NULL;
  } else {
    Cdata = (VOID *) ((UINT8 *)CdataHdr + sizeof (CDATA_HEADER) + sizeof (CDATA_COND) * CdataHdr->ConditionNum);
  }

  return Cdata;
}

/**
  Get a full CFGDATA set length.

  @param[in] ConfBlob    Pointer to configuration data blob to be looked at.

  @retval   Length of a full CFGDATA set.
            0 indicates no CFGDATA exists.

**/
UINT32
EFIAPI
GetConfigDataSize (
  VOID    *ConfBlob
)
{
  UINT32               Start;
  UINT32               Offset;
  UINT32               CfgBlobHdrLen;
  UINT32               PidMask;
  UINT32               CondVal;
  CDATA_BLOB          *LdrCfgBlob;
  CDATA_HEADER        *CdataHdr;

  LdrCfgBlob  = (CDATA_BLOB *) ConfBlob;
  if (LdrCfgBlob == NULL || LdrCfgBlob->Signature != CFG_DATA_SIGNATURE) {
    return 0;
  }

  CfgBlobHdrLen = LdrCfgBlob->HeaderLength;
  if (LdrCfgBlob->ExtraInfo.InternalDataOffset == 0) {
    // No  internal CFGDATA
    Start   = CfgBlobHdrLen;
    PidMask = MAX_UINT32; // Enforce true for simplicity (1 << GetPlatformId ());
  } else {
    // Has internal CFGDATA
    Start   = LdrCfgBlob->ExtraInfo.InternalDataOffset * 4;
    PidMask = BIT0;
  }
  Offset = Start;
  while (Offset < LdrCfgBlob->UsedLength) {
    CdataHdr = (CDATA_HEADER *) ((UINT8 *)LdrCfgBlob + Offset);
    CondVal  = CdataHdr->Condition[0].Value;
    if ((CondVal != 0) && ((CondVal & PidMask) == 0)) {
      break;
    }
    Offset += (CdataHdr->Length << 2);
  }

  return Offset - Start + CfgBlobHdrLen;
}
