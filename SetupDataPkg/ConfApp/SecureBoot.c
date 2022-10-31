/** @file
  Test driver to locate conf data from variable storage and print all contained entries.

  Copyright (c) 2017 - 2020, Intel Corporation. All rights reserved.<BR>
  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <UefiSecureBoot.h>
#include <Guid/ImageAuthentication.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/SecureBootVariableLib.h>
#include <Library/MuSecureBootKeySelectorLib.h>
#include <Library/SecureBootKeyStoreLib.h>
#include <Library/ResetSystemLib.h>

#include "ConfApp.h"

ConfAppKeyOptions  SecureBootClearTemplate = {
  .KeyName             = L"0",
  .KeyNameTextAttr     = EFI_TEXT_ATTR (EFI_YELLOW, EFI_BLACK),
  .Description         = L"None.\n",
  .DescriptionTextAttr = EFI_TEXT_ATTR (EFI_WHITE, EFI_BLACK),
  .UnicodeChar         = '0',
  .ScanCode            = SCAN_NULL,
  .EndState            = SecureBootClear
};

ConfAppKeyOptions  SecureBootEnrollTemplate = {
  .KeyName             = L"1",
  .KeyNameTextAttr     = EFI_TEXT_ATTR (EFI_YELLOW, EFI_BLACK),
  .Description         = L"None.\n",
  .DescriptionTextAttr = EFI_TEXT_ATTR (EFI_WHITE, EFI_BLACK),
  .UnicodeChar         = '1',
  .ScanCode            = SCAN_NULL,
  .EndState            = SecureBootEnroll
};

ConfAppKeyOptions  SecureBootEscTemplate = {
  .KeyName             = L"ESC",
  .KeyNameTextAttr     = EFI_TEXT_ATTR (EFI_YELLOW, EFI_BLACK),
  .Description         = L"Return to main menu.",
  .DescriptionTextAttr = EFI_TEXT_ATTR (EFI_WHITE, EFI_BLACK),
  .UnicodeChar         = CHAR_NULL,
  .ScanCode            = SCAN_ESC,
  .EndState            = SecureBootExit
};

UINTN              mSecBootOptionCount   = 0;
ConfAppKeyOptions  *mSecBootStateOptions = NULL;
UINT8              mSelectedKeyIndex     = MU_SB_CONFIG_NONE;
CHAR16             *mKeyNameBuffer       = NULL;
SecureBootState_t  mSecBootState         = SecureBootInit;
UINTN              mCurrentState         = (UINTN)-1;

/**
  Helper internal function to reset all local variable in this file.
**/
STATIC
VOID
ResetGlobals (
  VOID
  )
{
  mSecBootOptionCount = 0;
  if (mSecBootStateOptions != NULL) {
    FreePool (mSecBootStateOptions);
    mSecBootStateOptions = NULL;
  }

  mSelectedKeyIndex = MU_SB_CONFIG_NONE;
  if (mKeyNameBuffer != NULL) {
    FreePool (mKeyNameBuffer);
    mKeyNameBuffer = NULL;
  }

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
  EFI_STATUS         Status;
  UINTN              Index;
  UINT8              EnrollTextColor;
  UINT8              ClearTextColor;
  SecureBootState_t  EnrollEndState;
  SecureBootState_t  ClearEndState;

  PrintScreenInit ();
  Print (L"Secure Boot Options:\n");

  gST->ConOut->SetAttribute (gST->ConOut, EFI_TEXT_ATTR (EFI_WHITE, EFI_BLACK));
  Print (L"Current Status:\t\t");

  mCurrentState = GetCurrentSecureBootConfig ();
  if (mCurrentState == MU_SB_CONFIG_NONE) {
    gST->ConOut->SetAttribute (gST->ConOut, EFI_TEXT_ATTR (EFI_RED, EFI_BLACK));
    Print (L"None\n");
  } else if (mCurrentState == MU_SB_CONFIG_UNKNOWN) {
    gST->ConOut->SetAttribute (gST->ConOut, EFI_TEXT_ATTR (EFI_BLUE, EFI_BLACK));
    Print (L"Unknown\n");
  } else {
    gST->ConOut->SetAttribute (gST->ConOut, EFI_TEXT_ATTR (EFI_GREEN, EFI_BLACK));
    Print (L"%s\n", mSecureBootKeys[mCurrentState].SecureBootKeyName);
  }

  Print (L"\n");

  EnrollTextColor = SecureBootEnrollTemplate.DescriptionTextAttr;
  EnrollEndState  = SecureBootEnrollTemplate.EndState;
  ClearTextColor  = SecureBootClearTemplate.DescriptionTextAttr;
  ClearEndState   = SecureBootClearTemplate.EndState;
  if ((mCurrentState != MU_SB_CONFIG_NONE) && IsPostReadyToBoot ()) {
    gST->ConOut->SetAttribute (gST->ConOut, EFI_TEXT_ATTR (EFI_YELLOW, EFI_BLACK));
    Print (L"Post ready to boot, below options are view only:\n");
    EnrollTextColor = EFI_TEXT_ATTR (EFI_DARKGRAY, EFI_BLACK);
    ClearTextColor  = EFI_TEXT_ATTR (EFI_DARKGRAY, EFI_BLACK);
    EnrollEndState  = SecureBootError;
    ClearEndState   = SecureBootError;
  }

  mSecBootOptionCount  = mSecureBootKeysCount + 2; // Two extra options for clear and exit
  mSecBootStateOptions = AllocatePool (sizeof (ConfAppKeyOptions) * mSecBootOptionCount);
  mKeyNameBuffer       = AllocatePool ((sizeof (CHAR16) * 2) * mSecBootOptionCount);
  for (Index = 0; Index < mSecureBootKeysCount; Index++) {
    CopyMem (&mSecBootStateOptions[Index], &SecureBootEnrollTemplate, sizeof (ConfAppKeyOptions));
    mSecBootStateOptions[Index].Description         = mSecureBootKeys[Index].SecureBootKeyName;
    mKeyNameBuffer[Index * 2]                       = L'0' + (CHAR16)Index;
    mKeyNameBuffer[Index * 2 + 1]                   = L'\0';
    mSecBootStateOptions[Index].KeyName             = &mKeyNameBuffer[Index * 2];
    mSecBootStateOptions[Index].UnicodeChar         = '0' + (CHAR16)Index;
    mSecBootStateOptions[Index].DescriptionTextAttr = EnrollTextColor;
    mSecBootStateOptions[Index].EndState            = EnrollEndState;
  }

  CopyMem (&mSecBootStateOptions[Index], &SecureBootClearTemplate, sizeof (ConfAppKeyOptions));
  mKeyNameBuffer[Index * 2]                       = L'0' + (CHAR16)Index;
  mKeyNameBuffer[Index * 2 + 1]                   = L'\0';
  mSecBootStateOptions[Index].KeyName             = &mKeyNameBuffer[Index * 2];
  mSecBootStateOptions[Index].UnicodeChar         = '0' + (CHAR16)Index;
  mSecBootStateOptions[Index].DescriptionTextAttr = ClearTextColor;
  mSecBootStateOptions[Index].EndState            = ClearEndState;

  Index++;
  CopyMem (&mSecBootStateOptions[Index], &SecureBootEscTemplate, sizeof (ConfAppKeyOptions));

  Status = PrintAvailableOptions (mSecBootStateOptions, mSecBootOptionCount);
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
  EFI_STATUS    Status = EFI_SUCCESS;
  EFI_KEY_DATA  KeyData;

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
        Status = CheckSupportedOptions (&KeyData, mSecBootStateOptions, mSecBootOptionCount, (UINT32 *)&mSecBootState);
        if (Status == EFI_NOT_FOUND) {
          Status = EFI_SUCCESS;
        } else if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a Error processing incoming keystroke - %r\n", __FUNCTION__, Status));
          ASSERT (FALSE);
        } else if (mSecBootState == SecureBootEnroll) {
          mSelectedKeyIndex = (UINT8)(KeyData.Key.UnicodeChar - '0');
          if (mSelectedKeyIndex >= mSecureBootKeysCount) {
            DEBUG ((DEBUG_ERROR, "%a The selected key does not exist - %d\n", __FUNCTION__, mSelectedKeyIndex));
            Status = EFI_BUFFER_TOO_SMALL;
            ASSERT (FALSE);
          }
        }
      }

      break;
    case SecureBootClear:
      DEBUG ((DEBUG_INFO, "Selected clear Secure Boot Key\n"));
      if (mCurrentState != MU_SB_CONFIG_NONE) {
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
    case SecureBootEnroll:
      DEBUG ((DEBUG_INFO, "Selected %s\n", mSecureBootKeys[mSelectedKeyIndex].SecureBootKeyName));
      if (mCurrentState != mSelectedKeyIndex) {
        // First wipe off existing variables if it is enrolled somehow
        if (mCurrentState != MU_SB_CONFIG_NONE) {
          Status = DeleteSecureBootVariables ();
          if (EFI_ERROR (Status)) {
            break;
          }
        }

        Status = SetSecureBootConfig (mSelectedKeyIndex);
        if (!EFI_ERROR (Status)) {
          mSecBootState = SecureBootConfChange;
        } else {
          mSecBootState = SecureBootInit;
        }
      } else {
        mSecBootState = SecureBootWait;
      }

      break;
    case SecureBootError:
      Print (L"Cannot change secure boot settings post security boundary!\n");
      mSecBootState = SecureBootWait;
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
