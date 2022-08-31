/* @file ConfigSystemModeLibNull.c

  Library Instance for system mode related functions for configuration modules.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Uefi.h>

/**
  This routine indicates if the system is in Manufacturing Mode.

  @retval  ManufacturingMode - Platforms may have a manufacturing mode.
                               Configuration update will only be allowed in
                               such mode. Return TRUE if the device is in
                               Manufacturing Mode.
**/
BOOLEAN
EFIAPI
IsSystemInManufacturingMode (
  VOID
  )
{
  return FALSE;
}
