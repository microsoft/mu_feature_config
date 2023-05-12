/** @file
  Mock implementation of the MM Services Table Library.

  Copyright (C) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiMm.h>

extern EFI_MM_SYSTEM_TABLE  MockMmServices;

EFI_MM_SYSTEM_TABLE  *gMmst = &MockMmServices;