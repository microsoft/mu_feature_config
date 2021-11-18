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
#include <Library/MuUefiVersionLib.h>
#include <Library/UefiLib.h>
#include <Library/ConfigDataLib.h>

#include "ConfApp.h"

typedef enum {
  SysInfoInit,
  SysInfoWait,
  SysInfoExit,
  SysInfoMax
} SysInfoState_t;

#define SYS_INFO_STATE_OPTIONS      1

CONST ConfAppKeyOptions SysInfoStateOptions[SYS_INFO_STATE_OPTIONS] = {
  {
    .KeyName                = L"ESC",
    .KeyNameTextAttr        = EFI_TEXT_ATTR (EFI_YELLOW, EFI_BLACK),
    .Description            = L"Return to main menu.",
    .DescriptionTextAttr    = EFI_TEXT_ATTR (EFI_WHITE, EFI_BLACK),
    .UnicodeChar            = CHAR_NULL,
    .ScanCode               = SCAN_ESC,
    .EndState               = SysInfoExit
  }
};

SysInfoState_t mSysInfoState = SysInfoInit;
UINTN          mDateTimeCol = 0;
UINTN          mDateTimeRow = 0;
UINTN          mEndCol = 0;
UINTN          mEndRow = 0;

/**
  Helper internal function to reset all local variable in this file.
**/
STATIC
VOID
ResetGlobals (
  VOID
  )
{
  mSysInfoState = SysInfoInit;
  mDateTimeCol = 0;
  mDateTimeRow = 0;
  mEndCol = 0;
  mEndRow = 0;
}

/**
  Helper internal function to print UEFI version.
**/
EFI_STATUS
PrintVersion (
  VOID
  )
{
  EFI_STATUS  Status;
  CHAR16      *Buffer = NULL;
  UINTN       Length = 0;

  Status = GetBuildDateStringUnicode (NULL, &Length);
  Buffer = AllocateZeroPool ((Length + 1) * sizeof (CHAR16));
  if (Buffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = GetBuildDateStringUnicode (Buffer, &Length);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a Failed to get build date string - %r\n", __FUNCTION__, Status));
    ASSERT (FALSE);
    return Status;
  }

  Status = Print (L"System Version:\t%s\n", Buffer);
  FreePool (Buffer);
  return Status;
}

/**
  Helper internal function to query and print configuration data to ConOut.
**/
EFI_STATUS
PrintSettings (
  VOID
  )
{
  EFI_STATUS            Status;
  CHAR8                 *Buffer = NULL;
  UINTN                 Length = 0;
  UINT8                 Dummy;

  if (mSettingAccess == NULL) {
    return EFI_NOT_STARTED;
  }

  Print (L"Identity:\t");
  if (mIdMask == DFCI_IDENTITY_INVALID) {
    Print (L"Invalid ");
  }
  if (mIdMask & DFCI_IDENTITY_LOCAL) {
    Print (L"Local ");
  }
  if (mIdMask == DFCI_IDENTITY_SIGNER_ZTD) {
    Print (L"ZTD ");
  }
  if (mIdMask == DFCI_IDENTITY_SIGNER_USER2) {
    Print (L"User2 ");
  }
  if (mIdMask == DFCI_IDENTITY_SIGNER_USER1) {
    Print (L"User1 ");
  }
  if (mIdMask == DFCI_IDENTITY_SIGNER_USER) {
    Print (L"User ");
  }
  if (mIdMask == DFCI_IDENTITY_SIGNER_OWNER) {
    Print (L"Owner ");
  }
  Print (L"\n");

  Status = mSettingAccess->Get (mSettingAccess,
                                DFCI_OEM_SETTING_ID__CONF,
                                &mAuthToken,
                                DFCI_SETTING_TYPE_BINARY,
                                &Length,
                                &Dummy,
                                NULL);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    DEBUG ((DEBUG_ERROR, "%a Unexpected result to get settings - %r\n", __FUNCTION__, Status));
    return Status;
  }

  Buffer = AllocatePool (Length * sizeof (CHAR8));
  if (Buffer == NULL) {
    DEBUG ((DEBUG_ERROR, "%a Cannot allocate buffer of 0x%x to hold settings\n", __FUNCTION__, Length));
    goto Exit;
  }

  Status = mSettingAccess->Get (mSettingAccess,
                                DFCI_OEM_SETTING_ID__CONF,
                                &mAuthToken,
                                DFCI_SETTING_TYPE_BINARY,
                                &Length,
                                Buffer,
                                NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a Failed to get settings - %r\n", __FUNCTION__, Status));
    goto Exit;
  }

  Print (L"System Settings:\n");
  Print (L"%a\n", Buffer);

Exit:
  if (Buffer != NULL) {
    FreePool (Buffer);
  }
  return Status;
}

/**
  Helper internal function to print system date and time.
**/
EFI_STATUS
PrintDateTime (
  VOID
  )
{
  EFI_STATUS  Status;
  EFI_TIME    CurrentTime;

  Status = gRT->GetTime(&CurrentTime, NULL);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  gST->ConOut->SetCursorPosition(gST->ConOut, mDateTimeCol, mDateTimeRow);
  gST->ConOut->SetAttribute (gST->ConOut, EFI_TEXT_ATTR (EFI_WHITE, EFI_BLACK));
  Print (L"Date & Time:\t%02d/%02d/%04d - %02d:%02d:%02d\n",
         CurrentTime.Month,
         CurrentTime.Day,
         CurrentTime.Year,
         CurrentTime.Hour,
         CurrentTime.Minute,
         CurrentTime.Second
         );
  if (mEndCol != 0 || mEndRow != 0) {
    gST->ConOut->SetCursorPosition(gST->ConOut, mEndCol, mEndRow);
  }

  return Status;
}

/**
  Helper function to print system information to ConOut.
**/
EFI_STATUS
PrintSysInfoOptions (
  VOID
  )
{
  EFI_STATUS Status;

  PrintScreenInit ();
  Print(L"System Information:\n\n");
  // Collect what is needed and print in this step
  Status = PrintVersion ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = PrintSettings ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a Error printing settings data - %r\n", __FUNCTION__, Status));
    return Status;
  }

  mDateTimeCol = gST->ConOut->Mode->CursorColumn;
  mDateTimeRow = gST->ConOut->Mode->CursorRow;
  Status = PrintDateTime ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Print(L"\n");
  Status = PrintAvailableOptions (SysInfoStateOptions, SYS_INFO_STATE_OPTIONS);
  if (EFI_ERROR (Status)) {
    ASSERT (FALSE);
  }
  mEndCol = gST->ConOut->Mode->CursorColumn;
  mEndRow = gST->ConOut->Mode->CursorRow;

  return Status;
}

/**
  State machine for system information page. It will display fundamental information, including
  UEFI version, system time, DFCI identity and configuration settings.

  @retval EFI_SUCCESS           This iteration of state machine proceeds successfully.
  @retval Others                Failed to wait for valid keystrokes or application of
                                new configuration data failed.
**/
EFI_STATUS
EFIAPI
SysInfoMgr (
  VOID
  )
{
  EFI_STATUS          Status = EFI_SUCCESS;
  EFI_KEY_DATA        KeyData;

  switch (mSysInfoState) {
    case SysInfoInit:
      Status = PrintSysInfoOptions ();
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a Error occurred while printing system information - %r\n", __FUNCTION__, Status));
        ASSERT (FALSE);
        break;
      }
      mSysInfoState = SysInfoWait;
      break;
    case SysInfoWait:
      // Wait for key stroke event.
      //
      Status = PollKeyStroke (TRUE, 200000, &KeyData);
      if (Status == EFI_TIMEOUT) {
        Status = PrintDateTime ();
      } else if (Status == EFI_NOT_READY) {
        // There is no key entered, this might be due to terminal sends ack characters, do nothing
      } else if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a Waiting for keystroke failed at system info page - %r\n", __FUNCTION__, Status));
        ASSERT (FALSE);
      } else {
        Status = CheckSupportedOptions (&KeyData, SysInfoStateOptions, SYS_INFO_STATE_OPTIONS, (UINTN*)&mSysInfoState);
        if (Status == EFI_NOT_FOUND) {
          Status = EFI_SUCCESS;
        } else if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a Error processing incoming keystroke - %r\n", __FUNCTION__, Status));
          ASSERT (FALSE);
        }
      }
      break;
    case SysInfoExit:
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
