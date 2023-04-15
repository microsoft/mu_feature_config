/* @file MockActiveProfileIndexSelectorLib.c

  Mock instance for ActiveProfileIndexSelectorLib.c

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
  Return which profile is the active profile for this boot.
  This function validates the profile GUID is valid.

  @param[out] ActiveProfileIndex  The index for the active profile. A value of MAX_UINT32, when combined with a return
                                  value of EFI_SUCCESS, indicates that the default profile has been chosen. If the
                                  return value is not EFI_SUCCESS, the value of ActiveProfileIndex shall not be updated.

  @retval EFI_INVALID_PARAMETER   Input argument is null.
  @retval EFI_NO_RESPONSE         The source of truth for profile selection has returned a garbage value or not replied.
  @retval EFI_SUCCESS             The operation succeeds and ActiveProfileIndex contains the valid active profile
                                  index for this boot.
**/
EFI_STATUS
EFIAPI
GetActiveProfileIndex (
  OUT UINT32  *ActiveProfileIndex
  )
{
  *ActiveProfileIndex = (UINT32)mock ();

  return mock ();
}
