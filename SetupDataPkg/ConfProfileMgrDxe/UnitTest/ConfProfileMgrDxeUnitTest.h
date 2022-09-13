/** @file
  Header file for unit test to include functions to be tested from
  Configuration Data Setting Provider module.

  Copyright (C) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef CONF_PROFILE_MGR_DXE_UNIT_TEST_H_
#define CONF_PROFILE_MGR_DXE_UNIT_TEST_H_

/**
  This driver will query the active configuration profile and validate the flash contents match. If not, the
  profile contents will be written to flash and the system reset.

  @param  ImageHandle  Handle to this image.
  @param  SystemTable  Pointer to the system table

  @retval  EFI_SUCCESS   The driver validated the profile against flash contents successfully.
  @retval  !EFI_SUCCESS  The driver failed to validate the profile against flash contents successfully.
**/
EFI_STATUS
EFIAPI
ConfProfileMgrDxeEntry (
  IN  EFI_HANDLE        ImageHandle,
  IN  EFI_SYSTEM_TABLE  *SystemTable
  );

#endif // CONF_PROFILE_MGR_DXE_UNIT_TEST_H_
