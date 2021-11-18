/** @file
  Config data library instance for data access.

  Copyright (c) 2017 - 2020, Intel Corporation. All rights reserved.<BR>
  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/


#ifndef __CONFIGURATION_DATA_H__
#define __CONFIGURATION_DATA_H__

#include <Protocol/Policy.h>

#define CFG_DATA_SIGNATURE  SIGNATURE_32 ('C', 'F', 'G', 'D')

#define CDATA_BLOB_ATTR_SIGNED  (1 << 0)
#define CDATA_BLOB_ATTR_MERGED  (1 << 7)

#define CDATA_FLAG_TYPE_MASK    (3 << 0)
#define CDATA_FLAG_TYPE_NORMAL  (0 << 0)
#define CDATA_FLAG_TYPE_ARRAY   (1 << 0)
#define CDATA_FLAG_TYPE_REFER   (2 << 0)

#define CFG_LOAD_SRC_PDR        (1 << 0)
#define CFG_LOAD_SRC_BIOS       (1 << 1)

#define PID_TO_MASK(x)          (1 << ((x) & 0x1F))

#define CDATA_NO_TAG            0x000
#define CDATA_PLATFORMID_TAG    0x0F0

#define CDATA_NV_VAR_NAME       L"CONF_POLICY_BLOB"
#define CDATA_NV_VAR_ATTR       (EFI_VARIABLE_NON_VOLATILE|EFI_VARIABLE_BOOTSERVICE_ACCESS)

// Device setting for OEM usage to register settings provider
#define DFCI_OEM_SETTING_ID__CONF   "Device.ConfigData.ConfigData"

typedef struct {
  UINT16   PlatformId;
  UINT16   Reserved;
} PLATFORMID_CFG_DATA;

typedef struct {
  UINT16   PlatformId;
  UINT16   Tag        : 12;
  UINT16   IsInternal : 1;
  UINT16   Reserved   : 3;
} REFERENCE_CFG_DATA;

typedef struct {
  UINT32  Value;    // Bit masks on supported platforms
} CDATA_COND;

typedef struct {
  UINT32  ConditionNum   :  2;      // [1:0]   #of condition words present
  UINT32  Length         : 10;      // [11:2]  total size of item (in dword)
  UINT32  Flags          :  4;      // [15:12] unused/reserved so far
  UINT32  Version        :  4;      // [19:16] item (payload) format version
  UINT32  Tag            : 12;      // [31:20] identifies item (in payload)
  CDATA_COND     Condition[0];
} CDATA_HEADER;

typedef struct {
  UINT32  Signature;
  //
  // This header Length
  //
  UINT8   HeaderLength;
  UINT8   Attribute;
  union {
    //
    // Internal configuration data offset in DWORD from the start of data blob.
    // This value is only valid in runtime.
    //
    UINT16  InternalDataOffset;
    //
    // Security version number
    // This is available only in flash. It would be overwritten by CDATA_BLOB.InternalDataOffset in runtime
    //
    UINT8   Svn;
  } ExtraInfo;
  //
  // The total valid configuration data length including this header.
  //
  UINT32  UsedLength;
  //
  // The total space for configration data
  //
  UINT32  TotalLength;
} CDATA_BLOB;

typedef struct {

  /* header size */
  UINT8                       HeaderSize;

  /* base table ID */
  UINT8                       BaseTableId;

  /* size in byte for every array entry */
  UINT16                      ItemSize;

  /* array entry count */
  UINT16                      ItemCount;

  /* array entry ID bit offset */
  UINT8                       ItemIdBitOff;

  /* array entry ID bit length */
  UINT8                       ItemIdBitLen;

  /* array entry valid bit offset */
  UINT8                       ItemValidBitOff;

  /* unused */
  UINT8                       ItemUnused;

  /* array entry bit mask, 1 bit per entry to indicate the entry exists or not */
  UINT8                       BaseTableBitMask[0];

} ARRAY_CFG_HDR;

/**
  This function is supplied with a buffer of configuration data defined according platform
  YAML files. Platform should derive a printable string based on supplied data.

  @param[in]  Data            Pointer to platform configuration data.
  @param[in]  Size            The size of Data.
  @param[out] AsciiBuffer     When supplied, the printable string should be copied
                              into this buffer.
  @param[out] AsciiBufferSize When supplied, on input, this indicate the available size of
                              AsciiBuffer. On output, this should be the size of printable
                              string, including NULL terminator.

  @retval EFI_SUCCESS           The string is successfully serialized.
  @retval EFI_INVALID_PARAMETER The input data does not comply with platform configuration format.
  @retval EFI_BUFFER_TOO_SMALL  The supplied buffer is too small to fit in output.
  @retval EFI_UNSUPPORTED       This function is not supported for this instance.
  @retval Others                Other errors occurred.
**/
EFI_STATUS
EFIAPI
DumpConfigData (
  IN  VOID      *Data,
  IN  UINTN     Size,
  OUT VOID      *AsciiBuffer OPTIONAL,
  OUT UINTN     *AsciiBufferSize OPTIONAL
  );

/**
  This function is supplied with a buffer of configuration data defined according to 
  platform YAML files. Platform should update the applicable silicon policies.

  @param[in]  PolicyInterface Pointer to current policy protocol/PPI interface.
  @param[in]  Data            Pointer to platform configuration data.
  @param[in]  Size            The size of Data.

  @retval EFI_SUCCESS           The configuration is successfully applied to policy.
  @retval EFI_INVALID_PARAMETER The input data does not comply with platform configuration format,
                                or the policy interface is NULL.
  @retval EFI_UNSUPPORTED       This function is not supported for this instance.
  @retval Others                Other errors occurred.
**/
EFI_STATUS
EFIAPI
ProcessIncomingConfigData (
  IN POLICY_PROTOCOL  *PolicyInterface,
  IN VOID             *Data,
  IN UINTN            Size
  );

#endif
