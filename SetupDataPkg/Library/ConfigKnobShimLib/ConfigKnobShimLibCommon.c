/** @file
  Common functionality for the library interface for an OEM config policy creator to call into to fetch overrides
  for config values.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/ConfigKnobShimLib.h>
#include "ConfigKnobShimLibCommon.h"

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
  )
{
  EFI_STATUS  Status;
  UINTN       VariableSize = 0;

  if ((ConfigKnobGuid == NULL) || (ConfigKnobName == NULL) || (ConfigKnobData == NULL) ||
      (ConfigKnobDataSize == 0))
  {
    DEBUG ((DEBUG_ERROR, "%a: Invalid parameter!\n", __func__));
    Status = EFI_INVALID_PARAMETER;
    goto Exit;
  }

  // Check size in variable storage
  Status = GetConfigKnobFromVariable (ConfigKnobGuid, ConfigKnobName, NULL, &VariableSize);

  if ((Status != EFI_BUFFER_TOO_SMALL) && EFI_ERROR (Status)) {
    goto Exit;
  }

  if (ConfigKnobDataSize != VariableSize) {
    // we will only accept this variable if it is the correct size
    Status = EFI_BAD_BUFFER_SIZE;
  } else if (Status == EFI_BUFFER_TOO_SMALL) {
    // buffer too small means we found the variable and it was the size we expected
    // Check if it is in variable storage
    Status = GetConfigKnobFromVariable (ConfigKnobGuid, ConfigKnobName, ConfigKnobData, &VariableSize);
  }

Exit:
  if (EFI_ERROR (Status)) {
    // we didn't find the override in variable storage, which is expected if the knob has not been overridden, or
    // the size mismatched. Only debug verbose here as this is expected to happen in the majority of cases.
    DEBUG ((
      DEBUG_VERBOSE,
      "%a: failed to find override for config knob %s with status %r. Expected size: %u, found size: %u.\n",
      __func__,
      ConfigKnobName,
      Status,
      ConfigKnobDataSize,
      VariableSize
      ));
  }

  return Status;
}
