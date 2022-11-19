/** @file
  Library interface for the autogen XML config header to call into to fetch a config value when the system
  is in MFCI manufacturing mode. In MFCI customer mode, the autogen XML config header contains the default values
  for each configuration profile and will return those values.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <PiPei.h>
#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/PeiServicesLib.h>
#include <Ppi/ReadOnlyVariable2.h>

/**
  GetConfigKnob returns the cached configuration knob if it is already cached, otherwise the config knob
  is fetched from variable storage and cached in a policy.

  If the config knob is not found in variable storage or the policy cache, this function returns EFI_NOT_FOUND
  and NULL in ConfigKnobData. In this case, the autogen header will return the default value for the profile.

  This function is only expected to be called from the autogen header code, all consumers of config knobs are
  expected to use the getter functions in the autogen header.

  @param[in]  ConfigKnobGuid        The GUID of the requested config knob.
  @param[in]  ConfigKnobName        The name of the requested config knob.
  @param[out] ConfigKnobData        The retrieved data of the requested config knob. The caller will allocate memory for
                                    this buffer and is responsible for freeing it. In the case of EFI_NOT_FOUND, this
                                    buffer will not be changed.
  @param[in out] ConfigKnobDataSize The allocated size of ConfigKnobData. This is expected to be set to the correct
                                    value for the size of ConfigKnobData. If this size is too small for the config knob,
                                    EFI_BUFFER_TOO_SMALL will be returned and this value will be updated to the
                                    correct size. The caller will need to reallocate ConfigKnobData to this size and
                                    call the autogen getter function again.

  @retval EFI_INVALID_PARAMETER   Input argument is null.
  @retval EFI_NOT_FOUND           The requested config knob was not found in the policy cache or variable storage. This
                                  is expected when the config knob has not been updated from the profile default.
  @retval EFI_BUFFER_TOO_SMALL    ConfigKnobDataSize as passed in was too small for this config knob. The value has
                                  been updated to the correct value.
  @retval EFI_SUCCESS             The operation succeeds.

**/
EFI_STATUS
EFIAPI
GetConfigKnob (
  IN EFI_GUID   *ConfigKnobGuid,
  IN CHAR16     *ConfigKnobName,
  OUT VOID      *ConfigKnobData,
  IN OUT UINTN  *ConfigKnobDataSize
  )
{
  EFI_STATUS                       Status;
  EFI_PEI_READ_ONLY_VARIABLE2_PPI  *PPIVariableServices;

  if ((ConfigKnobGuid == NULL) || (ConfigKnobName == NULL) || (ConfigKnobData == NULL) ||
      (ConfigKnobDataSize == NULL))
  {
    DEBUG ((DEBUG_ERROR, "%a: Invalid parameter!\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

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
      __FUNCTION__,
      Status,
      ConfigKnobName
      ));

    return Status;
  }

  Status = PPIVariableServices->GetVariable (
                                  PPIVariableServices,
                                  ConfigKnobName,
                                  ConfigKnobGuid,
                                  NULL,
                                  ConfigKnobDataSize,
                                  ConfigKnobData
                                  );

  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_INFO,
      "%a: failed to find config knob %a with status %r."
      " This is expected if the knob has the default value or a too small buffer size was passed in\n",
      __FUNCTION__,
      ConfigKnobName,
      Status
      ));
  }

  return Status;
}
