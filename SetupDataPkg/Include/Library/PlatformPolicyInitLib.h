/** @file
  Library interface for platform to publish default policy interface.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef PLATFORM_POLICY_INIT_LIB_H_
#define PLATFORM_POLICY_INIT_LIB_H_

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
  );

#endif
