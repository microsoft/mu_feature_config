/** @file ConfigKnobShimPeiLib.c
  Library interface for an OEM config policy creator to call into to fetch overrides for config values.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <PiPei.h>
#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/PeiServicesLib.h>
#include <Ppi/ReadOnlyVariable2.h>

#include "../ConfigKnobShimLibCommon.h"

/**
  GetConfigKnobFromVariable returns the configuration knob from variable storage if it exists. This function is
  abstracted to work with PEI, DXE, and Standalone MM.

  This function is only expected to be called by GetConfigKnobOverride.

  @param[in]  ConfigKnobGuid        The GUID of the requested config knob.
  @param[in]  ConfigKnobName        The name of the requested config knob.
  @param[out] ConfigKnobData        The retrieved data of the requested config knob. The caller will allocate memory for
                                    this buffer and is responsible for freeing it. This parameter is acceptable to be
                                    NULL if the data size is all that is requested.
  @param[in out] ConfigKnobDataSize The allocated size of ConfigKnobData. This is expected to be set to the correct
                                    value for the size of ConfigKnobData. If this size is too small for the config knob,
                                    EFI_BUFFER_TOO_SMALL will be returned. This represents a mismatch in the profile
                                    expected size and what is stored in variable storage, so the profile value will
                                    take precedence.

  @retval EFI_NOT_FOUND           The requested config knob was not found in the policy cache or variable storage. This
                                  is expected when the config knob has not been updated from the profile default.
  @retval EFI_BUFFER_TOO_SMALL    ConfigKnobDataSize as passed in was too small for this config knob. This is expected
                                  if stale variables exist in flash.
  @retval EFI_NOT_READY           Variable Services not available.
  @retval !EFI_SUCCESS            Failed to read variable from variable storage.
  @retval EFI_SUCCESS             The operation succeeds.

**/
EFI_STATUS
GetConfigKnobFromVariable (
  IN EFI_GUID   *ConfigKnobGuid,
  IN CHAR16     *ConfigKnobName,
  OUT VOID      *ConfigKnobData,
  IN OUT UINTN  *ConfigKnobDataSize
  )
{
  EFI_STATUS                       Status;
  EFI_PEI_READ_ONLY_VARIABLE2_PPI  *PPIVariableServices;

  Status = PeiServicesLocatePpi (
             &gEfiPeiReadOnlyVariable2PpiGuid,
             0,
             NULL,
             (VOID **)&PPIVariableServices
             );

  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to locate variable services with status %r, falling back to profile value for config knob %a\n",
      __func__,
      Status,
      ConfigKnobName
      ));

    return Status;
  }

  return PPIVariableServices->GetVariable (
                                PPIVariableServices,
                                ConfigKnobName,
                                ConfigKnobGuid,
                                NULL,
                                ConfigKnobDataSize,
                                ConfigKnobData
                                );
}
