/** @file
  Consumer driver to locate conf data from variable storage and print all contained entries.

  Potentially we will install DFCI settings provider here as well.

  Copyright (c) 2017 - 2020, Intel Corporation. All rights reserved.<BR>
  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/ConfigDataLib.h>
#include <Library/ConfigBlobBaseLib.h>

/**
  Find configuration data header by its tag and platform ID.

  @param[in] ConfDataPtr  Pointer to configuration data.

  @retval EFI_INVALID_PARAMETER   Input argument is null.
  @retval EFI_SUCCESS             All p.

**/
EFI_STATUS
IterateConfData (
  IN CONST VOID          *ConfDataPtr,
  IN SINGLE_TAG_HANDLER  SingleTagHandler
  )
{
  CDATA_BLOB          *CdataBlob;
  CDATA_HEADER        *CdataHdr;
  UINT8               Idx;
  REFERENCE_CFG_DATA  *Refer;
  UINT32              Offset;
  EFI_STATUS          Status;
  VOID                *Data;

  if (ConfDataPtr == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  CdataBlob = (CDATA_BLOB *)ConfDataPtr;
  if ((CdataBlob->Signature != CFG_DATA_SIGNATURE) ||
      (CdataBlob->HeaderLength > CdataBlob->TotalLength) ||
      (CdataBlob->HeaderLength > CdataBlob->UsedLength) ||
      (CdataBlob->UsedLength > CdataBlob->TotalLength))
  {
    return EFI_INVALID_PARAMETER;
  }

  Status = EFI_SUCCESS;
  Offset = CdataBlob->HeaderLength;
  DEBUG ((DEBUG_VERBOSE, "Config Blob Header\n"));
  DEBUG ((DEBUG_VERBOSE, "Signature:     0x%08X\n", CdataBlob->Signature));
  DEBUG ((DEBUG_VERBOSE, "HeaderLength:  0x%02X\n", CdataBlob->HeaderLength));
  DEBUG ((DEBUG_VERBOSE, "Attribute:     0x%02X\n", CdataBlob->Attribute));
  DEBUG ((DEBUG_VERBOSE, "ExtraInfo:     0x%04X\n", CdataBlob->ExtraInfo.InternalDataOffset));
  DEBUG ((DEBUG_VERBOSE, "TotalLength:   0x%08X\n", CdataBlob->TotalLength));
  DEBUG ((DEBUG_VERBOSE, "UsedLength:    0x%08X\n", CdataBlob->UsedLength));

  while (Offset < CdataBlob->UsedLength) {
    CdataHdr = (CDATA_HEADER *)((UINT8 *)CdataBlob + Offset);
    Data     = (VOID *)((UINT8 *)CdataHdr + sizeof (CDATA_HEADER) + sizeof (CDATA_COND) * CdataHdr->ConditionNum);

    DEBUG ((DEBUG_VERBOSE, "\tConfig Data Header\n"));
    DEBUG ((DEBUG_VERBOSE, "\tConditionNum:  0x%08X\n", CdataHdr->ConditionNum));
    DEBUG ((DEBUG_VERBOSE, "\tLength:        0x%08X\n", CdataHdr->Length));
    DEBUG ((DEBUG_VERBOSE, "\tFlags:         0x%08X\n", CdataHdr->Flags));
    DEBUG ((DEBUG_VERBOSE, "\tVersion:       0x%08X\n", CdataHdr->Version));
    DEBUG ((DEBUG_VERBOSE, "\tTag:           0x%08X\n", CdataHdr->Tag));
    DEBUG ((DEBUG_VERBOSE, "\tData:          0x%08p\n", Data));

    for (Idx = 0; Idx < CdataHdr->ConditionNum; Idx++) {
      DEBUG ((DEBUG_VERBOSE, "\tCondition %d:   0x%08X\n", Idx, CdataHdr->Condition[Idx]));
      if ((CdataHdr->Flags & CDATA_FLAG_TYPE_MASK) == CDATA_FLAG_TYPE_REFER) {
        Refer = (REFERENCE_CFG_DATA *)Data;
        DEBUG ((DEBUG_VERBOSE, "\t\tPlatformId:    0x%08X\n", Refer->PlatformId));
        DEBUG ((DEBUG_VERBOSE, "\t\tTag:           0x%08X\n", Refer->Tag));
        DEBUG ((DEBUG_VERBOSE, "\t\tIsInternal:    0x%08X\n", Refer->IsInternal));
        DEBUG ((DEBUG_VERBOSE, "\t\tReserved:      0x%08X\n", Refer->Reserved));
      } else if (((CdataHdr->Flags & CDATA_FLAG_TYPE_MASK) == CDATA_FLAG_TYPE_ARRAY) ||
                 ((CdataHdr->Flags & CDATA_FLAG_TYPE_MASK) == CDATA_FLAG_TYPE_NORMAL))
      {
        Status = SingleTagHandler (
                   CdataHdr->Tag,
                   Data,
                   (CdataHdr->Length << 2) - sizeof (*CdataHdr) - sizeof (CDATA_COND) * CdataHdr->ConditionNum
                   );
        if (EFI_ERROR (Status)) {
          ASSERT_EFI_ERROR (Status);
          break;
        }
      } else {
        Status = EFI_INVALID_PARAMETER;
        ASSERT (FALSE);
        break;
      }
    }

    Offset += (CdataHdr->Length << 2);
    DEBUG ((DEBUG_VERBOSE, "Offset:    0x%08X\n\n", Offset));
  }

  return Status;
}
