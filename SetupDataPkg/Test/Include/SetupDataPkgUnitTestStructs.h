/** @file
  Unit test structure definitions

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef SETUPDATAPKG_UNIT_TEST_STRUCTS_H_
#define SETUPDATAPKG_UNIT_TEST_STRUCTS_H_

#include <Uefi.h>

typedef struct _PPI_STATUS {
  VOID          *Ppi;
  EFI_STATUS    Status;
} PPI_STATUS;

typedef struct _MM_PROTOCOL_STATUS {
  VOID          *Protocol;
  EFI_STATUS    Status;
} MM_PROTOCOL_STATUS;

#endif // SETUPDATAPKG_UNIT_TEST_STRUCTS_H_
