/** @file
  NULL instance for config data library.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Protocol/Policy.h>

/**
  This function will be invoked by policy producer during PEI phase.
  Platforms should publish default silicon policies at this stage for
  further usage.

  @param[in]  PolicyInterface Pointer to current policy protocol/PPI interface.

  @retval EFI_SUCCESS           The policy is successfully initialized.
  @retval EFI_INVALID_PARAMETER The policy interface is NULL.
  @retval EFI_UNSUPPORTED       This function is not supported for this instance.
  @retval Others                Other errors occurred.
**/
EFI_STATUS
EFIAPI
PlatformPolicyInit (
  IN POLICY_PROTOCOL  *PolicyProtocol
  )
{
  return EFI_UNSUPPORTED;
}
