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

#define CONFIG_INCLUDE_CACHE
#include <Generated/ConfigClientGenerated.h>
#include <Generated/ConfigServiceGenerated.h>

#define CONFIG_KNOB_NAME_MAX_LENGTH  64

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
  UINTN       VariableSize = ConfigKnobDataSize;

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

C_ASSERT (sizeof (EFI_GUID) == sizeof (config_guid_t));

// Get the raw knob value
//
// This function is called by the generated functions and uses the generated knob metadata to call into the generic
// GetConfigKnob function to get the knob value.
void *
get_knob_value (
  knob_t  knob
  )
{
  ASSERT (knob < KNOB_MAX);
  knob_data_t  *knob_data = &g_knob_data[(int)knob];

  // Convert the name to a CHAR16 string
  CHAR16  name[CONFIG_KNOB_NAME_MAX_LENGTH];
  int     i = 0;

  for (i = 0; i < CONFIG_KNOB_NAME_MAX_LENGTH; i++) {
    name[i] = (CHAR16)knob_data->name[i];
    if (knob_data->name[i] == '\0') {
      break;
    }
  }

  // The name should be null terminated
  ASSERT (i < CONFIG_KNOB_NAME_MAX_LENGTH);

  // Get the knob value
  EFI_STATUS  result = GetConfigKnob (
                         (EFI_GUID *)&knob_data->vendor_namespace,
                         name,
                         knob_data->cache_value_address,
                         knob_data->value_size,
                         (void *)knob_data->default_value_address,
                         knob_data->value_size
                         );

  // This function should not fail for us
  // If the knob is not found in storage, or has the wrong size or attributes, it will return the default value
  // The only failure cases are invalid parameters, which should not happen
  ASSERT (result == EFI_SUCCESS);

  // Validate the value from flash meets the constraints of the knob
  if (knob_data->validator != NULL) {
    if (!knob_data->validator (knob_data->cache_value_address)) {
      // If it doesn't, we will set the value to the default value
      DEBUG ((DEBUG_ERROR, "Config knob %a failed validation!\n", knob_data->name));
      CopyMem (knob_data->cache_value_address, knob_data->default_value_address, knob_data->value_size);
    }
  }

  // Return a pointer to the data, the generated functions will cast this to the correct type
  return knob_data->cache_value_address;
}
