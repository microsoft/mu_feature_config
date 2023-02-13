/** @file ConfigKnobShimLib.h
  Library interface for an OEM config policy creator to call into to fetch overrides for config values.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef CONFIG_KNOB_SHIM_LIB_H_
#define CONFIG_KNOB_SHIM_LIB_H_

/**
  GetConfigKnobOverride searches for an override to the given config knob.

  If the config override is found and the data size matches ConfigKnobDataSize, ConfigKnobData will be written with
  the override value.

  If the config override is not found or another error occurs, ConfigKnobData will not be written with the override
  value.

  This function is only expected to be called from an OEM config policy creator.

  @param[in]  ConfigKnobGuid      The GUID of the requested config knob.
  @param[in]  ConfigKnobName      The name of the requested config knob.
  @param[out] ConfigKnobData      The retrieved data of the requested config knob. The caller will allocate memory
                                  for this buffer and the caller is responsible for freeing it.
  @param[in] ConfigKnobDataSize   The allocated size of ConfigKnobData. This will be set to the correct value for the
                                  size of ConfigKnobData in the success case.

  @retval EFI_INVALID_PARAMETER   Input argument is null.
  @retval EFI_BAD_BUFFER_SIZE     Override data size does not match config knob data size.
  @retval EFI_NOT_FOUND           Override not found for this knob.
  @retval !EFI_SUCCESS            Other error in finding an override to this config knob
  @retval EFI_SUCCESS             The operation succeeds.

**/
EFI_STATUS
EFIAPI
GetConfigKnobOverride (
  IN EFI_GUID  *ConfigKnobGuid,
  IN CHAR16    *ConfigKnobName,
  OUT VOID     *ConfigKnobData,
  IN UINTN     ConfigKnobDataSize
  );

#endif // CONFIG_KNOB_SHIM_LIB_H_
