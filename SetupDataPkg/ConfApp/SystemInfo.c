/** @file
  Test driver to locate conf data from variable storage and print all contained entries.

  Copyright (c) 2017 - 2020, Intel Corporation. All rights reserved.<BR>
  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Protocol/FirmwareManagement.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/MuUefiVersionLib.h>
#include <Library/UefiLib.h>

#include "ConfApp.h"

#define SYS_INFO_STATE_OPTIONS  1

CONST ConfAppKeyOptions  SysInfoStateOptions[SYS_INFO_STATE_OPTIONS] = {
  {
    .KeyName             = L"ESC",
    .KeyNameTextAttr     = EFI_TEXT_ATTR (EFI_YELLOW, EFI_BLACK),
    .Description         = L"Return to main menu.",
    .DescriptionTextAttr = EFI_TEXT_ATTR (EFI_WHITE, EFI_BLACK),
    .UnicodeChar         = CHAR_NULL,
    .ScanCode            = SCAN_ESC,
    .EndState            = SysInfoExit
  }
};

SysInfoState_t  mSysInfoState = SysInfoInit;
UINTN           mDateTimeCol  = 0;
UINTN           mDateTimeRow  = 0;
UINTN           mEndCol       = 0;
UINTN           mEndRow       = 0;

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
  mDateTimeCol  = 0;
  mDateTimeRow  = 0;
  mEndCol       = 0;
  mEndRow       = 0;
}

/**
  Helper internal function to print UEFI version.
**/
EFI_STATUS
PrintVersion (
  VOID
  )
{
  EFI_STATUS                        Status;
  EFI_FIRMWARE_MANAGEMENT_PROTOCOL  **FmpList;
  UINTN                             FmpCount;
  UINTN                             Index;
  EFI_FIRMWARE_MANAGEMENT_PROTOCOL  *Fmp;
  UINTN                             DescriptorSize;
  EFI_FIRMWARE_IMAGE_DESCRIPTOR     *FmpImageInfoBuf;
  UINT8                             FmpImageInfoCount;
  UINT32                            FmpImageInfoDescriptorVer;
  UINTN                             ImageInfoSize;
  UINT32                            PackageVersion;
  CHAR16                            *PackageVersionName;

  FmpImageInfoBuf    = NULL;
  Fmp                = NULL;
  FmpList            = NULL;
  PackageVersionName = NULL;

  //
  // Get all FMP instances and then use the descriptor to get string name and version
  //
  Status = EfiLocateProtocolBuffer (&gEfiFirmwareManagementProtocolGuid, &FmpCount, (VOID *)&FmpList);
  if (EFI_ERROR (Status) && (Status != EFI_NOT_FOUND)) {
    DEBUG ((DEBUG_ERROR, "EfiLocateProtocolBuffer(gEfiFirmwareManagementProtocolGuid) returned error.  %r \n", Status));
    goto Done;
  } else if (Status == EFI_NOT_FOUND) {
    Print (L"No Firmware Management Protocols Installed!\n");
    Status = EFI_SUCCESS;
    goto Done;
  }

  Status = Print (L"Firmware Versions:\n");

  for (Index = 0; Index < FmpCount; Index++) {
    Fmp = (EFI_FIRMWARE_MANAGEMENT_PROTOCOL *)(FmpList[Index]);
    // get the GetImageInfo for the FMP

    ImageInfoSize = 0;
    //
    // get necessary descriptor size
    // this should return TOO SMALL
    Status = Fmp->GetImageInfo (
                    Fmp,                        // FMP Pointer
                    &ImageInfoSize,             // Buffer Size (in this case 0)
                    NULL,                       // NULL so we can get size
                    &FmpImageInfoDescriptorVer, // DescriptorVersion
                    &FmpImageInfoCount,         // DescriptorCount
                    &DescriptorSize,            // DescriptorSize
                    &PackageVersion,            // PackageVersion
                    &PackageVersionName         // PackageVersionName
                    );

    if (Status != EFI_BUFFER_TOO_SMALL) {
      DEBUG ((DEBUG_ERROR, "%a - Unexpected Failure in GetImageInfo.  Status = %r\n", __func__, Status));
      continue;
    }

    FmpImageInfoBuf = NULL;
    FmpImageInfoBuf = AllocateZeroPool (ImageInfoSize);
    if (FmpImageInfoBuf == NULL) {
      DEBUG ((DEBUG_ERROR, "%a - Failed to get memory for descriptors.\n", __func__));
      continue;
    }

    PackageVersionName = NULL;
    Status             = Fmp->GetImageInfo (
                                Fmp,
                                &ImageInfoSize,             // ImageInfoSize
                                FmpImageInfoBuf,            // ImageInfo
                                &FmpImageInfoDescriptorVer, // DescriptorVersion
                                &FmpImageInfoCount,         // DescriptorCount
                                &DescriptorSize,            // DescriptorSize
                                &PackageVersion,            // PackageVersion
                                &PackageVersionName         // PackageVersionName
                                );

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a - Failure in GetImageInfo.  Status = %r\n", __func__, Status));
      goto FmpCleanUp;
    }

    if (FmpImageInfoCount == 0) {
      DEBUG ((DEBUG_INFO, "%a - No Image Info descriptors.\n", __func__));
      goto FmpCleanUp;
    }

    if (FmpImageInfoCount > 1) {
      DEBUG ((DEBUG_INFO, "%a - Found %d descriptors.  For config app we only show the 1st descriptor.\n", __func__, FmpImageInfoCount));
    }

    if (FmpImageInfoBuf->ImageIdName != NULL) {
      Status = Print (L"\t%s:\t", FmpImageInfoBuf->ImageIdName);
    } else {
      DEBUG ((DEBUG_ERROR, "%a - FMP ImageIdName is null\n", __func__));
    }

    if (FmpImageInfoBuf->VersionName != NULL) {
      Status = Print (L"%s\n", FmpImageInfoBuf->VersionName);
    } else {
      DEBUG ((DEBUG_ERROR, "%a - FMP VersionName is null\n", __func__));
    }

FmpCleanUp:
    // clean up -
    FreePool (FmpImageInfoBuf);
    FmpImageInfoBuf = NULL;
    if (PackageVersionName != NULL) {
      FreePool (PackageVersionName);
      PackageVersionName = NULL;
    }
  } // for loop for all fmp handles

Done:
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

  Status = gRT->GetTime (&CurrentTime, NULL);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  gST->ConOut->SetCursorPosition (gST->ConOut, mDateTimeCol, mDateTimeRow);
  gST->ConOut->SetAttribute (gST->ConOut, EFI_TEXT_ATTR (EFI_WHITE, EFI_BLACK));
  Print (
    L"Date & Time:\t%02d/%02d/%04d - %02d:%02d:%02d\n",
    CurrentTime.Month,
    CurrentTime.Day,
    CurrentTime.Year,
    CurrentTime.Hour,
    CurrentTime.Minute,
    CurrentTime.Second
    );
  if ((mEndCol != 0) || (mEndRow != 0)) {
    gST->ConOut->SetCursorPosition (gST->ConOut, mEndCol, mEndRow);
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
  EFI_STATUS  Status;

  PrintScreenInit ();
  Print (L"System Information:\n\n");
  // Collect what is needed and print in this step
  Status = PrintVersion ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  mDateTimeCol = gST->ConOut->Mode->CursorColumn;
  mDateTimeRow = gST->ConOut->Mode->CursorRow;
  Status       = PrintDateTime ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Print (L"\n");
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
  UEFI version, system time, and configuration settings.

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
  EFI_STATUS    Status = EFI_SUCCESS;
  EFI_KEY_DATA  KeyData;

  switch (mSysInfoState) {
    case SysInfoInit:
      Status = PrintSysInfoOptions ();
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a Error occurred while printing system information - %r\n", __func__, Status));
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
        DEBUG ((DEBUG_ERROR, "%a Waiting for keystroke failed at system info page - %r\n", __func__, Status));
        ASSERT (FALSE);
      } else {
        Status = CheckSupportedOptions (&KeyData, SysInfoStateOptions, SYS_INFO_STATE_OPTIONS, (UINT32 *)&mSysInfoState);
        if (Status == EFI_NOT_FOUND) {
          Status = EFI_SUCCESS;
        } else if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "%a Error processing incoming keystroke - %r\n", __func__, Status));
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
