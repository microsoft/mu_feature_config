/** @file
  Test driver to locate conf data from variable storage and print all contained entries.

  Copyright (c) 2017 - 2020, Intel Corporation. All rights reserved.<BR>
  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <UefiSecureBoot.h>
#include <DfciSystemSettingTypes.h>
#include <Guid/MuVarPolicyFoundationDxe.h>
#include <Protocol/DfciAuthentication.h>

#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/ResetSystemLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/SecureBootKeyStoreLib.h>
#include <Library/PerformanceLib.h>
#include <Library/ConfigSystemModeLib.h>

#include "ConfApp.h"

#define MAIN_STATE_OPTIONS  6

CONST ConfAppKeyOptions  MainStateOptions[MAIN_STATE_OPTIONS] = {
  {
    .KeyName             = L"1",
    .KeyNameTextAttr     = EFI_TEXT_ATTR (EFI_YELLOW, EFI_BLACK),
    .Description         = L"Show System Information.",
    .DescriptionTextAttr = EFI_TEXT_ATTR (EFI_WHITE, EFI_BLACK),
    .UnicodeChar         = '1',
    .ScanCode            = SCAN_NULL,
    .EndState            = SystemInfo
  },
  {
    .KeyName             = L"2",
    .KeyNameTextAttr     = EFI_TEXT_ATTR (EFI_YELLOW, EFI_BLACK),
    .Description         = L"Configure Secure Boot.",
    .DescriptionTextAttr = EFI_TEXT_ATTR (EFI_WHITE, EFI_BLACK),
    .UnicodeChar         = '2',
    .ScanCode            = SCAN_NULL,
    .EndState            = SecureBoot
  },
  {
    .KeyName             = L"3",
    .KeyNameTextAttr     = EFI_TEXT_ATTR (EFI_YELLOW, EFI_BLACK),
    .Description         = L"Boot Options.",
    .DescriptionTextAttr = EFI_TEXT_ATTR (EFI_WHITE, EFI_BLACK),
    .UnicodeChar         = '3',
    .ScanCode            = SCAN_NULL,
    .EndState            = BootOption
  },
  {
    .KeyName             = L"4",
    .KeyNameTextAttr     = EFI_TEXT_ATTR (EFI_YELLOW, EFI_BLACK),
    .Description         = L"Update Setup Configuration.\n",
    .DescriptionTextAttr = EFI_TEXT_ATTR (EFI_WHITE, EFI_BLACK),
    .UnicodeChar         = '4',
    .ScanCode            = SCAN_NULL,
    .EndState            = SetupConf
  },
  {
    .KeyName             = L"h",
    .KeyNameTextAttr     = EFI_TEXT_ATTR (EFI_YELLOW, EFI_BLACK),
    .Description         = L"Reprint this menu.",
    .DescriptionTextAttr = EFI_TEXT_ATTR (EFI_WHITE, EFI_BLACK),
    .UnicodeChar         = 'h',
    .ScanCode            = SCAN_NULL,
    .EndState            = MainInit
  },
  {
    .KeyName             = L"ESC",
    .KeyNameTextAttr     = EFI_TEXT_ATTR (EFI_YELLOW, EFI_BLACK),
    .Description         = L"Exit this menu and reboot system.",
    .DescriptionTextAttr = EFI_TEXT_ATTR (EFI_WHITE, EFI_BLACK),
    .UnicodeChar         = CHAR_NULL,
    .ScanCode            = SCAN_ESC,
    .EndState            = MainExit
  }
};

ConfState_t                        mConfState       = MainInit;
EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL  *mSimpleTextInEx = NULL;
DFCI_SETTING_ACCESS_PROTOCOL       *mSettingAccess  = NULL;
DFCI_AUTH_TOKEN                    mAuthToken;
DFCI_IDENTITY_MASK                 mIdMask;          // Identities installed
SECURE_BOOT_PAYLOAD_INFO           *mSecureBootKeys;
UINT8                              mSecureBootKeysCount;

/**
  Quick helper function to see if ReadyToBoot has already been signalled.

  @retval     TRUE    ReadyToBoot has been signalled.
  @retval     FALSE   Otherwise...

**/
BOOLEAN
IsPostReadyToBoot (
  VOID
  )
{
  EFI_STATUS       Status;
  UINT32           Attributes;
  PHASE_INDICATOR  Indicator;
  UINTN            Size;
  BOOLEAN          Result = FALSE;

  Size = sizeof (Indicator);

  Status = gRT->GetVariable (
                  READY_TO_BOOT_INDICATOR_VAR_NAME,
                  &gMuVarPolicyDxePhaseGuid,
                  &Attributes,
                  &Size,
                  &Indicator
                  );
  Result = (!EFI_ERROR (Status) && (Attributes == READY_TO_BOOT_INDICATOR_VAR_ATTR));

  return Result;
} // IsPostReadyToBoot()

/**
  Polling function for key that was pressed.

  @param[in]  EnableTimeOut     Flag indicating whether a timeout is needed to wait for key stroke.
  @param[in]  TimeOutInterval   Timeout specified to wait for key stroke, in the unit of 100ns.
  @param[out] KeyData           Return the key that was pressed.

  @retval EFI_SUCCESS     The operation was successful.
  @retval EFI_NOT_READY   The read key occurs without a keystroke.
  @retval EFI_TIMEOUT     If supplied, this will be returned if specified time interval has expired.
**/
EFI_STATUS
EFIAPI
PollKeyStroke (
  IN  BOOLEAN       EnableTimeOut,
  IN  UINTN         TimeOutInterval OPTIONAL,
  OUT EFI_KEY_DATA  *KeyData
  )
{
  EFI_EVENT   WaitHandle[2] = { NULL, NULL };
  UINTN       Index;
  EFI_STATUS  Status;
  UINTN       CountOfEvents = 1;

  if (mSimpleTextInEx == NULL) {
    // Something must be off. We should have initialized this at the entry
    ASSERT (FALSE);
    return EFI_DEVICE_ERROR;
  }

  if (KeyData == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  WaitHandle[0] = mSimpleTextInEx->WaitForKeyEx;

  if (EnableTimeOut) {
    Status = gBS->CreateEvent (EVT_TIMER, 0, NULL, NULL, &WaitHandle[1]);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to create event = %r.\n", Status));
      return Status;
    }

    Status = gBS->SetTimer (
                    WaitHandle[1],
                    TimerRelative,
                    (UINT64)TimeOutInterval
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a Failed to set timer for keystroke timeout event - %r\n", __FUNCTION__, Status));
      ASSERT (FALSE);
      return Status;
    }

    CountOfEvents++;
  }

  Status = gBS->WaitForEvent (CountOfEvents, WaitHandle, &Index);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Error from WaitForEvent. Code = %r.\n", Status));
    goto Exit;
  }

  if (EnableTimeOut && (Index != 0)) {
    Status = EFI_TIMEOUT;
    goto Exit;
  }

  Status = mSimpleTextInEx->ReadKeyStrokeEx (mSimpleTextInEx, KeyData);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Error from ReadKeyStrokeEx. Code = %r.\n", Status));
    goto Exit;
  }

Exit:
  if (WaitHandle[1]) {
    gBS->CloseEvent (WaitHandle[1]);
  }

  return Status;
}

/**
  Helper function to clear screen and initialize the cursor position and color attributes.
**/
VOID
PrintScreenInit (
  VOID
  )
{
  gST->ConOut->ClearScreen (gST->ConOut);
  gST->ConOut->SetCursorPosition (gST->ConOut, 5, 5);
  gST->ConOut->SetAttribute (gST->ConOut, EFI_TEXT_ATTR (EFI_WHITE, EFI_BLACK));
}

/**
  Function to request the main page to reset. This should be invoked before any
  subsystem return to main page.
**/
VOID
EFIAPI
ExitSubRoutine (
  VOID
  )
{
  mConfState = MainInit;
}

/**
  Helper function to print available options at the console.

  @param[in]  KeyOptions      Available key options supported by this state machine.
  @param[in]  OptionCount     The number of available options (including padding lines).

  @retval EFI_SUCCESS             The operation was successful.
  @retval EFI_INVALID_PARAMETER   The input pointer is NULL.
**/
EFI_STATUS
EFIAPI
PrintAvailableOptions (
  IN  CONST ConfAppKeyOptions  *KeyOptions,
  IN  UINTN                    OptionCount
  )
{
  UINTN  Index = 0;

  if (KeyOptions == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  for (Index = 0; Index < OptionCount; Index++) {
    if (KeyOptions[Index].KeyName) {
      gST->ConOut->SetAttribute (gST->ConOut, KeyOptions[Index].KeyNameTextAttr);
      Print (L"%s)\t\t", KeyOptions[Index].KeyName);
    }

    if (KeyOptions[Index].Description) {
      gST->ConOut->SetAttribute (gST->ConOut, KeyOptions[Index].DescriptionTextAttr);
      Print (L"%s\n", KeyOptions[Index].Description);
    }
  }

  return EFI_SUCCESS;
}

/**
  Helper function to wait for supported key options and transit state.

  @param[in]  KeyData       Keystroke read from ConIn.
  @param[in]  KeyOptions    Available key options supported by this state machine.
  @param[in]  OptionCount   The number of available options (including padding lines).
  @param[in]  State         Pointer to machine state intended to transit.

  @retval EFI_SUCCESS             The operation was successful.
  @retval EFI_INVALID_PARAMETER   The input pointer is NULL.
  @retval EFI_NOT_FOUND           The input keystroke is not supported by supplied KeyData.
**/
EFI_STATUS
EFIAPI
CheckSupportedOptions (
  IN  EFI_KEY_DATA             *KeyData,
  IN  CONST ConfAppKeyOptions  *KeyOptions,
  IN  UINTN                    OptionCount,
  IN  UINT32                   *State
  )
{
  UINTN       Index  = 0;
  EFI_STATUS  Status = EFI_NOT_FOUND;

  if ((KeyOptions == NULL) || (State == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  for (Index = 0; Index < OptionCount; Index++) {
    if ((KeyOptions[Index].UnicodeChar == CHAR_NULL) &&
        (KeyOptions[Index].ScanCode == SCAN_NULL))
    {
      // Print place holder, do nothing
      continue;
    }

    if (KeyOptions[Index].EndState == MAX_UINTN) {
      continue;
    }

    if ((KeyOptions[Index].UnicodeChar == KeyData->Key.UnicodeChar) &&
        (KeyOptions[Index].ScanCode == KeyData->Key.ScanCode))
    {
      // This is a match
      *State = KeyOptions[Index].EndState;
      Status = EFI_SUCCESS;
      break;
    }
  }

  return Status;
}

/**
  Acquire an Auth Token and save it in a protocol

  @param[in]  PasswordBuffer      Pointer to buffer that holds password.

  @retval EFI_SUCCESS         The operation was successful.
  @retval EFI_NOT_READY       Failed to get valid auth token.
  @retval Other               Failed to get enrolled identities.
**/
EFI_STATUS
GetAuthTokenAndIdentities (
  CONST IN  CHAR16  *PasswordBuffer
  )
{
  EFI_STATUS                    Status;
  DFCI_AUTHENTICATION_PROTOCOL  *AuthProtocol;

  Status = gBS->LocateProtocol (
                  &gDfciAuthenticationProtocolGuid,
                  NULL,
                  (VOID **)&AuthProtocol
                  );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a - Failed to locate MsAuthProtocol. Can't use check auth. %r\n", __FUNCTION__, Status));
    AuthProtocol = NULL;
    return Status;
  }

  if (PasswordBuffer != NULL) {
    Status = AuthProtocol->AuthWithPW (AuthProtocol, PasswordBuffer, StrLen (PasswordBuffer), &mAuthToken);
    DEBUG ((DEBUG_INFO, "%a Auth Token Acquired %x - %r\n", __FUNCTION__, mAuthToken, Status));
  } else {
    Status = AuthProtocol->AuthWithPW (AuthProtocol, NULL, 0, &mAuthToken);
    DEBUG ((DEBUG_INFO, "%a Auth Token Acquired with NULL Password %x - %r\n", __FUNCTION__, mAuthToken, Status));
  }

  if (!EFI_ERROR (Status) && (mAuthToken == DFCI_AUTH_TOKEN_INVALID)) {
    DEBUG ((DEBUG_ERROR, "%a Auth Token is invalid %r\n", __FUNCTION__, Status));
    Status = EFI_NOT_READY;
    goto Exit;
  }

  Status = AuthProtocol->GetEnrolledIdentities (AuthProtocol, &mIdMask);
  if (!EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get owner ids. %r\n", __FUNCTION__, Status));
    goto Exit;
  }

Exit:
  return Status;
}

/**
  Entrypoint of configuration app. This function holds the main state machine for
  console based user interface.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some error occurs when executing this entry point.
**/
EFI_STATUS
EFIAPI
ConfAppEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS    Status;
  EFI_KEY_DATA  KeyData;

  // First thing first, try to see tell the operation mode
  if (!IsSystemInManufacturingMode ()) {
    // If not, we will not allow updating configuration or secure boot keys
    PERF_EVENT_SIGNAL_BEGIN (&gEfiEventReadyToBootGuid);
    EfiSignalEventReadyToBoot ();
    PERF_EVENT_SIGNAL_END (&gEfiEventReadyToBootGuid);
  }

  gBS->SetWatchdogTimer (0x0000, 0x0000, 0x0000, NULL);  // Cancel watchdog in case booted, as opposed to running in shell

  gST->ConOut->EnableCursor (gST->ConOut, FALSE);

  DEBUG ((DEBUG_INFO, "%a - Entry...\n", __FUNCTION__));

  // Get the Simple Text Ex protocol on the ConsoleIn handle to get the "x" to terminate.
  //
  Status = gBS->HandleProtocol (
                  gST->ConsoleInHandle,
                  &gEfiSimpleTextInputExProtocolGuid,
                  (VOID **)&mSimpleTextInEx
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to locate SimpleTextIn on ConIn. Code = %r.\n", Status));
    goto Exit;
  }

  Status = gBS->LocateProtocol (
                  &gDfciSettingAccessProtocolGuid,
                  NULL,
                  (VOID **)&mSettingAccess
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to locate SettingAccess. Code = %r.\n", Status));
    goto Exit;
  }

  Status = GetPlatformKeyStore (&mSecureBootKeys, &mSecureBootKeysCount);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to get platform secure boot keys. Code = %r.\n", Status));
    goto Exit;
  }

  // Force-connect all controllers.
  //
  EfiBootManagerConnectAll ();

  Status = GetAuthTokenAndIdentities (NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to get Auth token. Code = %r.\n", Status));
    goto Exit;
  }

  //
  // Main state machine
  //
  while (TRUE) {
    switch (mConfState) {
      case MainInit:
        PrintScreenInit ();
        Print (L"Available Options:\n");
        Print (L"\n");
        Status = PrintAvailableOptions (MainStateOptions, MAIN_STATE_OPTIONS);
        if (EFI_ERROR (Status)) {
          ASSERT (FALSE);
          goto Exit;
        }

        mConfState = MainWait;
        break;
      case MainWait:
        // Wait for key stroke event.
        //
        Status = PollKeyStroke (FALSE, 0, &KeyData);
        if (Status == EFI_NOT_READY) {
          Status = EFI_SUCCESS;
        } else if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a Error occurred while waiting for keystroke - %r\n", __FUNCTION__, Status));
          ASSERT (FALSE);
        } else {
          Status = CheckSupportedOptions (&KeyData, MainStateOptions, MAIN_STATE_OPTIONS, (UINT32 *)&mConfState);
          if (Status == EFI_NOT_FOUND) {
            Status = EFI_SUCCESS;
          } else if (EFI_ERROR (Status)) {
            DEBUG ((DEBUG_ERROR, "%a Error processing incoming keystroke - %r\n", __FUNCTION__, Status));
            ASSERT (FALSE);
          }
        }

        break;
      case SystemInfo:
        Status = SysInfoMgr ();
        break;
      case SecureBoot:
        Status = SecureBootMgr ();
        break;
      case BootOption:
        Status = BootOptionMgr ();
        break;
      case SetupConf:
        Status = SetupConfMgr ();
        break;
      case MainExit:
        Print (L"Please confirm to exit setup menu (N/y)...\n");
        // Wait for key stroke event.
        //
        Status = PollKeyStroke (FALSE, 0, &KeyData);
        if (Status == EFI_NOT_READY) {
          // Do nothing
        } else if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a Error occurred while waiting for exit confirmation - %r\n", __FUNCTION__, Status));
          ASSERT (FALSE);
        } else if ((KeyData.Key.UnicodeChar == 'y') ||
                   (KeyData.Key.UnicodeChar == 'Y'))
        {
          ResetCold ();
        } else {
          mConfState = MainInit;
        }

        break;
      default:
        // Should not happen
        DEBUG ((DEBUG_ERROR, "%a Unexpected state found - %x\n", __FUNCTION__, mConfState));
        ASSERT (FALSE);
        break;
    }

    if (EFI_ERROR (Status)) {
      // The failed step might have done residue in sub state machines, reset the system to start over.
      ASSERT (FALSE);
      ResetCold ();
      CpuDeadLoop ();
    }
  }

Exit:
  return Status;
}
