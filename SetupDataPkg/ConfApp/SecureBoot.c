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
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/MsSecureBootLib.h>
#include <Library/ResetSystemLib.h>

#include "ConfApp.h"

typedef enum {
  SecureBootInit,
  SecureBootWait,
  SecureBootClear,
  SecureBootMsft,
  SecureBootMsftPlus,
  SecureBootExit,
  SecureBootConfChange,
  SecureBootMax
} SecureBootState_t;

#define SEC_BOOT_CONF_STATE_OPTIONS      4

CONST ConfAppKeyOptions SecBootStateOptions[SEC_BOOT_CONF_STATE_OPTIONS] = {
  {
    .KeyName                = L"1",
    .KeyNameTextAttr        = EFI_TEXT_ATTR (EFI_YELLOW, EFI_BLACK),
    .Description            = L"Microsoft Only.",
    .DescriptionTextAttr    = EFI_TEXT_ATTR (EFI_WHITE, EFI_BLACK),
    .UnicodeChar            = '1',
    .ScanCode               = SCAN_NULL,
    .EndState               = SecureBootMsft
  },
  {
    .KeyName                = L"2",
    .KeyNameTextAttr        = EFI_TEXT_ATTR (EFI_YELLOW, EFI_BLACK),
    .Description            = L"Microsoft and 3rd Party.",
    .DescriptionTextAttr    = EFI_TEXT_ATTR (EFI_WHITE, EFI_BLACK),
    .UnicodeChar            = '2',
    .ScanCode               = SCAN_NULL,
    .EndState               = SecureBootMsftPlus
  },
  {
    .KeyName                = L"3",
    .KeyNameTextAttr        = EFI_TEXT_ATTR (EFI_YELLOW, EFI_BLACK),
    .Description            = L"None.\n",
    .DescriptionTextAttr    = EFI_TEXT_ATTR (EFI_WHITE, EFI_BLACK),
    .UnicodeChar            = '3',
    .ScanCode               = SCAN_NULL,
    .EndState               = SecureBootClear
  },
  {
    .KeyName                = L"ESC",
    .KeyNameTextAttr        = EFI_TEXT_ATTR (EFI_YELLOW, EFI_BLACK),
    .Description            = L"Return to main menu.",
    .DescriptionTextAttr    = EFI_TEXT_ATTR (EFI_WHITE, EFI_BLACK),
    .UnicodeChar            = CHAR_NULL,
    .ScanCode               = SCAN_ESC,
    .EndState               = SecureBootExit
  }
};

SecureBootState_t mSecBootState = SecureBootInit;
UINTN             mCurrentState = (UINTN)-1;

/**
  Helper internal function to reset all local variable in this file.
**/
STATIC
VOID
ResetGlobals (
  VOID
  )
{
  mSecBootState = SecureBootInit;
  mCurrentState = (UINTN)-1;
}

/**
  Helper function to print current Secure Boot status and available selections.

  @retval EFI_SUCCESS                   The options are successfully printed.
  @retval EFI_SECURITY_VIOLATION        Current system secure boot state is unknown.
**/
EFI_STATUS
PrintSBOptions (
  VOID
  )
{
  EFI_STATUS Status;

  PrintScreenInit ();
  Print (L"Secure Boot Options:\n");

  gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR (EFI_WHITE, EFI_BLACK));
  Print (L"Current Status:\t\t");

  mCurrentState = GetCurrentSecureBootConfig ();
  switch (mCurrentState) {
    case MS_SB_CONFIG_MS_ONLY:
      gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR (EFI_GREEN, EFI_BLACK));
      Print (L"Microsoft Only\n");
      break;
    case MS_SB_CONFIG_MS_3P:
      gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR (EFI_CYAN, EFI_BLACK));
      Print (L"Microsoft and 3rd Party\n");
      break;
    case MS_SB_CONFIG_NONE:
    // Intentionally fall through
    case (UINTN)-1:
      gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_ATTR (EFI_RED, EFI_BLACK));
      Print (L"None\n");
      break;
    default:
      DEBUG ((DEBUG_ERROR, "%a Unexpected secure boot configuration state - %x.\n", mCurrentState));
      ASSERT (FALSE);
      return EFI_SECURITY_VIOLATION;
  }
  Print (L"\n");

  Status = PrintAvailableOptions (SecBootStateOptions, SEC_BOOT_CONF_STATE_OPTIONS);
  if (EFI_ERROR (Status)) {
    ASSERT (FALSE);
  }
  return Status;
}

/**
  State machine for secure boot page. It will react to user input from keystroke
  to set selected secure boot option or go back to previous page.

  @retval EFI_SUCCESS           This iteration of state machine proceeds successfully.
  @retval Others                Failed to wait for valid keystrokes or failed to set
                                platform key to variable service.
**/
EFI_STATUS
EFIAPI
SecureBootMgr (
  VOID
  )
{
  EFI_STATUS          Status = EFI_SUCCESS;
  EFI_KEY_DATA        KeyData;

  switch (mSecBootState) {
    case SecureBootInit:
      // Collect what is needed and print in this step
      Status = PrintSBOptions ();
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a Error occurred while printing secure boot options - %r\n", __FUNCTION__, Status));
        ASSERT (FALSE);
        break;
      }
      mSecBootState = SecureBootWait;
      break;
    case SecureBootWait:
      // Wait for key stroke event.
      //
      Status = PollKeyStroke (FALSE, 0, &KeyData);
      if (Status == EFI_NOT_READY) {
        Status = EFI_SUCCESS;
      } else if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a Error occurred waiting for secure boot selections - %r\n", __FUNCTION__, Status));
        ASSERT (FALSE);
      } else {
        Status = CheckSupportedOptions (&KeyData, SecBootStateOptions, SEC_BOOT_CONF_STATE_OPTIONS, (UINTN*)&mSecBootState);
        if (Status == EFI_NOT_FOUND) {
          Status = EFI_SUCCESS;
        } else if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a Error processing incoming keystroke - %r\n", __FUNCTION__, Status));
          ASSERT (FALSE);
        }
      }
      break;
    case SecureBootClear:
      DEBUG ((DEBUG_INFO, "Selected clear Secure Boot Key\n"));
      if ((mCurrentState != MS_SB_CONFIG_NONE) &&
          (mCurrentState != (UINTN)-1)) {
        Status = DeleteSecureBootVariables ();
        if (!EFI_ERROR (Status)) {
          mSecBootState = SecureBootConfChange;
        } else {
          mSecBootState = SecureBootInit;
        }
      } else {
        mSecBootState = SecureBootWait;
      }
      break;
    case SecureBootMsft:
      DEBUG ((DEBUG_INFO, "Selected Microsoft ONLY Secure Boot Key\n"));
      if (mCurrentState != MS_SB_CONFIG_MS_ONLY) {
        Status = SetDefaultSecureBootVariables (FALSE);
        if (!EFI_ERROR (Status)) {
          mSecBootState = SecureBootConfChange;
        } else {
          mSecBootState = SecureBootInit;
        }
      } else {
        mSecBootState = SecureBootWait;
      }
      break;
    case SecureBootMsftPlus:
      DEBUG ((DEBUG_INFO, "Selected Microsoft and 3rd Party Secure Boot Key\n"));
      if (mCurrentState != MS_SB_CONFIG_MS_3P) {
        Status = SetDefaultSecureBootVariables (TRUE);
        if (!EFI_ERROR (Status)) {
          mSecBootState = SecureBootConfChange;
        } else {
          mSecBootState = SecureBootInit;
        }
      } else {
        mSecBootState = SecureBootWait;
      }
      break;
    case SecureBootExit:
      ResetGlobals ();
      ExitSubRoutine ();
      break;
    case SecureBootConfChange:
      ResetCold ();
      // Should not be here
      CpuDeadLoop ();
      break;
    default:
      ASSERT (FALSE);
      Status = EFI_DEVICE_ERROR;
      break;
  }

  return Status;
}
