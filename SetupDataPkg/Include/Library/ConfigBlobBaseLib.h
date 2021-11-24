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

#endif
