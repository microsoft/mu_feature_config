/** @file
  Test driver header for internal interfaces and definitions.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef CONF_APP_H_
#define CONF_APP_H_

#include <UefiSecureBoot.h>

#define NO_TRANSITION_STATE  MAX_UINTN

typedef enum ConfState_t_def {
  MainInit,
  MainWait,
  SystemInfo,
  SecureBoot,
  BootOption,
  SetupConf,
  MainExit,
  StateMax
} ConfState_t;

typedef enum SysInfoState_t_def {
  SysInfoInit,
  SysInfoWait,
  SysInfoExit,
  SysInfoMax
} SysInfoState_t;

typedef enum {
  SecureBootInit,
  SecureBootWait,
  SecureBootClear,
  SecureBootEnroll,
  SecureBootError,
  SecureBootExit,
  SecureBootConfChange,
  SecureBootMax
} SecureBootState_t;

typedef enum BootOptState_t_def {
  BootOptInit,
  BootOptWait,
  BootOptBootNow,
  BootOptReorder,
  BootOptExit,
  BootOptMax
} BootOptState_t;

typedef enum SetupConfState_t_def {
  SetupConfInit,
  SetupConfWait,
  SetupConfUpdateUsb,
  SetupConfUpdateSerialHint,
  SetupConfUpdateSerial,
  SetupConfDumpSerial,
  SetupConfDumpComplete,
  SetupConfExit,
  SetupConfError,
  SetupConfMax
} SetupConfState_t;

#pragma pack (push, 1)

typedef struct {
  CONST CHAR16    *KeyName;
  UINT8           KeyNameTextAttr;
  CONST CHAR16    *Description;
  UINT8           DescriptionTextAttr;
  CHAR16          UnicodeChar;
  CHAR16          ScanCode;
  UINT32          EndState;
} ConfAppKeyOptions;

STATIC_ASSERT (sizeof (UINT32) == sizeof (ConfState_t), "sizeof (UINT32) does not match sizeof (enum) in this environment");
// #define gConfAppResetGuid gEfiCallerIdGuid
#pragma pack (pop)

/**
  Quick helper function to see if ReadyToBoot has already been signalled.

  @retval     TRUE    ReadyToBoot has been signalled.
  @retval     FALSE   Otherwise...

**/
BOOLEAN
IsPostReadyToBoot (
  VOID
  );

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
  );

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
  );

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
  );

/**
  Function to request the main page to reset. This should be invoked before any
  subsystem return to main page.
**/
VOID
EFIAPI
ExitSubRoutine (
  VOID
  );

/**
  Helper function to clear screen and initialize the cursor position and color attributes.
**/
VOID
PrintScreenInit (
  VOID
  );

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
  );

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
  );

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
  );

/**
  State machine for configuration setup. It will react to user keystroke to accept
  configuration data from selected option.

  @retval EFI_SUCCESS           This iteration of state machine proceeds successfully.
  @retval Others                Failed to wait for valid keystrokes or application of
                                new configuration data failed.
**/
EFI_STATUS
EFIAPI
SetupConfMgr (
  VOID
  );

extern EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL  *mSimpleTextInEx;
extern SECURE_BOOT_PAYLOAD_INFO           *mSecureBootKeys;
extern UINT8                              mSecureBootKeysCount;

#endif // CONF_APP_H_
