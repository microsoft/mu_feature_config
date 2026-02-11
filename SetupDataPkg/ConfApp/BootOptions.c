/** @file
  Test driver to locate conf data from variable storage and print all contained entries.

  Copyright (c) 2017 - 2020, Intel Corporation. All rights reserved.<BR>
  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/UefiLib.h>
#include <Library/ResetUtilityLib.h>

#include "ConfApp.h"

#define STATIC_BOOT_OPTIONS  2

CONST ConfAppKeyOptions  StaticBootOptions[STATIC_BOOT_OPTIONS] = {
  {
    .KeyName             = NULL,
    .KeyNameTextAttr     = EFI_TEXT_ATTR (EFI_YELLOW, EFI_BLACK),
    .Description         = L"\n\tSelect Index to boot to the corresponding option.\n",
    .DescriptionTextAttr = EFI_TEXT_ATTR (EFI_WHITE, EFI_BLACK),
    .UnicodeChar         = CHAR_NULL,
    .ScanCode            = SCAN_NULL,
    .EndState            = MAX_UINT32
  },
  {
    .KeyName             = L"ESC",
    .KeyNameTextAttr     = EFI_TEXT_ATTR (EFI_YELLOW, EFI_BLACK),
    .Description         = L"Return to main menu.",
    .DescriptionTextAttr = EFI_TEXT_ATTR (EFI_WHITE, EFI_BLACK),
    .UnicodeChar         = CHAR_NULL,
    .ScanCode            = SCAN_ESC,
    .EndState            = BootOptExit
  }
};

BootOptState_t                mBootOptState    = BootOptInit;
EFI_BOOT_MANAGER_LOAD_OPTION  *mBootOptions    = NULL;
UINTN                         mBootOptionCount = 0;
UINT16                        mOpCandidate;
ConfAppKeyOptions             *mKeyOptions = NULL;
UINTN                         mOptionCount = 0;
CHAR16                        *mKeyNames   = NULL;

/**
  Helper internal function to reset all local variable in this file.
**/
STATIC
VOID
ResetGlobals (
  VOID
  )
{
  mBootOptState    = BootOptInit;
  mBootOptions     = NULL;
  mBootOptionCount = 0;
  mOpCandidate     = 0;

  mOptionCount = 0;
  if (mKeyOptions != NULL) {
    FreePool (mKeyOptions);
    mKeyOptions = NULL;
  }

  if (mKeyNames != NULL) {
    FreePool (mKeyNames);
    mKeyNames = NULL;
  }
}

/**
  Helper function to print registered boot options.

  Note: this function will not register new options.

  @retval EFI_SUCCESS                   This function always returns success.
**/
EFI_STATUS
PrintBootOptions (
  VOID
  )
{
  UINTN       OptionIndex;
  EFI_STATUS  Status;

  PrintScreenInit ();
  Print (L"Boot Options:\n");
  Print (L"\n");

  mBootOptions = EfiBootManagerGetLoadOptions (&mBootOptionCount, LoadOptionTypeBoot);
  mKeyOptions  = AllocatePool (sizeof (ConfAppKeyOptions) * (mBootOptionCount + STATIC_BOOT_OPTIONS));
  mKeyNames    = AllocatePool (sizeof (L"#####") * (mBootOptionCount));
  for (OptionIndex = 0; OptionIndex < mBootOptionCount; OptionIndex++) {
    UnicodeSPrint ((CHAR16 *)((CHAR8 *)mKeyNames + OptionIndex * sizeof (L"#####")), sizeof (L"#####"), L"%d", OptionIndex + 1);
    mKeyOptions[OptionIndex].KeyName             = (CHAR16 *)((CHAR8 *)mKeyNames + OptionIndex * sizeof (L"#####"));
    mKeyOptions[OptionIndex].KeyNameTextAttr     = EFI_TEXT_ATTR (EFI_YELLOW, EFI_BLACK);
    mKeyOptions[OptionIndex].Description         = mBootOptions[OptionIndex].Description;
    mKeyOptions[OptionIndex].DescriptionTextAttr = EFI_TEXT_ATTR (EFI_WHITE, EFI_BLACK);
    mKeyOptions[OptionIndex].UnicodeChar         = (CHAR16)(OptionIndex + '1');
    mKeyOptions[OptionIndex].ScanCode            = SCAN_NULL;
    mKeyOptions[OptionIndex].EndState            = BootOptBootNow;
  }

  CopyMem (&mKeyOptions[OptionIndex], StaticBootOptions, sizeof (ConfAppKeyOptions) * STATIC_BOOT_OPTIONS);

  mOptionCount = mBootOptionCount + STATIC_BOOT_OPTIONS;
  Status       = PrintAvailableOptions (mKeyOptions, mOptionCount);
  if (EFI_ERROR (Status)) {
    ASSERT (FALSE);
  }

  return EFI_SUCCESS;
}

/**
  State machine for boot option page. It will react to user input from keystroke
  to boot to selected boot option or go back to previous page.

  @retval EFI_SUCCESS           This iteration of state machine proceeds successfully.
  @retval Others                Failed to wait for valid keystrokes. Failed boot will not
                                return error code but cause reboot directly.
**/
EFI_STATUS
EFIAPI
BootOptionMgr (
  VOID
  )
{
  EFI_STATUS    Status;
  EFI_KEY_DATA  KeyData;

  Status = EFI_SUCCESS;

  switch (mBootOptState) {
    case BootOptInit:
      // Collect what is needed and print in this step
      Status = PrintBootOptions ();
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a Error occurred during printing boot options - %r\n", __func__, Status));
        ASSERT (FALSE);
        break;
      }

      mBootOptState = BootOptWait;
      break;
    case BootOptWait:
      // Wait for key stroke event. Boot option list should not change when here
      //
      Status = PollKeyStroke (FALSE, 0, &KeyData);
      if (Status == EFI_NOT_READY) {
        Status = EFI_SUCCESS;
      } else if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a Error occurred waiting for key stroke - %r\n", __func__, Status));
        ASSERT (FALSE);
      } else {
        Status = CheckSupportedOptions (&KeyData, mKeyOptions, mOptionCount, (UINT32 *)&mBootOptState);
        if (Status == EFI_NOT_FOUND) {
          Status = EFI_SUCCESS;
        } else if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a Error processing incoming keystroke - %r\n", __func__, Status));
          ASSERT (FALSE);
        } else {
          mOpCandidate = KeyData.Key.UnicodeChar - '1';
        }
      }

      break;
    case BootOptBootNow:
      DEBUG ((DEBUG_INFO, "Boot to Option %d - %s now!!!\n", mOpCandidate, mBootOptions[mOpCandidate].Description));
      EfiBootManagerBoot (mBootOptions + mOpCandidate);
      // If we ever come back, we should directly reboot since the state of system might have changed...
      // Prepare Reset GUID
      ResetSystemWithSubtype (EfiResetCold, &gConfAppResetGuid);
      CpuDeadLoop ();
      break;
    case BootOptExit:
      ResetGlobals ();
      ExitSubRoutine ();
      break;
    default:
      ASSERT (FALSE);
      Status = EFI_DEVICE_ERROR;
      break;
  }

  return Status;
}
