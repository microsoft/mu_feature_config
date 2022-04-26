/** @file
  Config data library instance for data access.

  Copyright (c) 2017 - 2020, Intel Corporation. All rights reserved.<BR>
  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef CONFIG_BLOB_H_
#define CONFIG_BLOB_H_

#include <Library/ConfigDataLib.h>

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
  );

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
  );

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
  );

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
  );

/**
  Get a full CFGDATA set length.

  @param[in] ConfBlob    Pointer to configuration data blob to be looked at.

  @retval   Length of a full CFGDATA set.
            0 indicates no CFGDATA exists.

**/
UINT32
EFIAPI
GetConfigDataSize (
  VOID  *ConfBlob
  );

#endif
