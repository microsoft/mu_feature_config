/* @file MockConfigSystemModeLib.c

  Mocked instance for system mode related functions for configuration modules.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

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
  return (BOOLEAN)mock ();
}
