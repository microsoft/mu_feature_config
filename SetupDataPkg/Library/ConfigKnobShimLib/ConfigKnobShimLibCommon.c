/** @file
  Common functionality for the library interface for the autogen XML config header to call into to fetch a config value
  when the system is in MFCI manufacturing mode. In MFCI customer mode, the autogen XML config header contains the
  default values for each configuration profile and will return those values.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>

#include "ConfigKnobShimLibCommon.h"

/**
  GetConfigKnob returns the cached configuration knob if it is already cached, otherwise the config knob
  is fetched from variable storage and cached in a policy.

  If the config knob is not found in variable storage or the policy cache, this function returns EFI_NOT_FOUND
  and NULL in ConfigKnobData. In this case, the autogen header will return the default value for the profile.

  This function is only expected to be called from the autogen header code, all consumers of config knobs are
  expected to use the getter functions in the autogen header.

  @param[in]  ConfigKnobGuid      The GUID of the requested config knob.
  @param[in]  ConfigKnobName      The name of the requested config knob.
  @param[out] ConfigKnobData      The retrieved data of the requested config knob. The caller will allocate memory
                                  for this buffer and the caller is responsible for freeing it.
  @param[in] ConfigKnobDataSize   The allocated size of ConfigKnobData. This will be set to the correct value for the
                                  size of ConfigKnobData in the success case. This should equal ProfileDefaultSize, if
                                  not, EFI_BAD_BUFFER_SIZE will be returned.
  @param[in] ProfileDefaultValue  The profile defined default value of this knob, to be filled in if knob is not
                                  overridden in a cached policy or variable storage.
  @param[in] ProfileDefaultSize

  @retval EFI_INVALID_PARAMETER   Input argument is null.
  @retval EFI_BAD_BUFFER_SIZE     Caller passed in a value of ConfigKnobDataSize that does not equal
                                  ProfileDefaultSize.
  @retval EFI_SUCCESS             The operation succeeds.

**/
EFI_STATUS
EFIAPI
GetConfigKnob (
  IN EFI_GUID  *ConfigKnobGuid,
  IN CHAR16    *ConfigKnobName,
  OUT VOID     *ConfigKnobData,
  IN UINTN     ConfigKnobDataSize,
  IN VOID      *ProfileDefaultValue,
  IN UINTN     ProfileDefaultSize
  )
{
  EFI_STATUS  Status;
  UINTN       VariableSize;

  if ((ConfigKnobGuid == NULL) || (ConfigKnobName == NULL) || (ConfigKnobData == NULL) ||
      (ConfigKnobDataSize == 0) || (ProfileDefaultValue == NULL) || (ProfileDefaultSize == 0))
  {
    DEBUG ((DEBUG_ERROR, "%a: Invalid parameter!\n", __FUNCTION__));
    Status = EFI_INVALID_PARAMETER;
    goto Exit;
  }

  if (ProfileDefaultSize != ConfigKnobDataSize) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: ConfigKnobDataSize %u does not equal ProfileDefaultSize %u\n",
      __FUNCTION__,
      ConfigKnobDataSize,
      ProfileDefaultSize
      ));
    Status = EFI_BAD_BUFFER_SIZE;
    goto Exit;
  }

  // Check if it is in variable storage
  Status = GetConfigKnobFromVariable (ConfigKnobGuid, ConfigKnobName, ConfigKnobData, &VariableSize);

  // TODO: Also check attributes here
  if (ConfigKnobDataSize != VariableSize) {
    // we will only accept this variable if it is the correct size
    Status = EFI_NOT_FOUND;
  }

  if (EFI_ERROR (Status)) {
    // we didn't find the value in variable storage, which is expected if the knob has not been overridden, or
    // the size mismatched. In either case, we default to the profile value.
    DEBUG ((
      DEBUG_VERBOSE,
      "%a: failed to find config knob %a with status %r. Expected size: %u, found size: %u."
      " Defaulting to profile defined value.\n",
      __FUNCTION__,
      ConfigKnobName,
      Status,
      ConfigKnobDataSize,
      VariableSize
      ));

    CopyMem (ConfigKnobData, ProfileDefaultValue, ProfileDefaultSize);

    // we failed to get the variable, but successfully returned the profile value, this is a success case
    Status = EFI_SUCCESS;
  }

Exit:
  return Status;
}
