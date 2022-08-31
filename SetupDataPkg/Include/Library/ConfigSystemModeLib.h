/** @file
  Library interface for system mode related functions for configuration modules.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef CONFIG_SYSTEM_MODE_LIB_H_
#define CONFIG_SYSTEM_MODE_LIB_H_

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
  );

#endif
