/** @file
  NULL instance for config data library.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Protocol/Policy.h>

/**
  This function is supplied with a buffer of configuration data defined according platform
  YAML files. Platform should derive a printable string based on supplied data.

  @param[in]  Data            Pointer to platform configuration data.
  @param[in]  Size            The size of Data.
  @param[out] AsciiBuffer     When supplied, the printable string should be copied
                              into this buffer.
  @param[out] AsciiBufferSize When supplied, on input, this indicate the available size of
                              AsciiBuffer. On output, this should be the size of printable
                              string, including NULL terminator.

  @retval EFI_SUCCESS           The string is successfully serialized.
  @retval EFI_INVALID_PARAMETER The input data does not comply with platform configuration format.
  @retval EFI_BUFFER_TOO_SMALL  The supplied buffer is too small to fit in output.
  @retval EFI_UNSUPPORTED       This function is not supported for this instance.
  @retval Others                Other errors occurred.
**/
EFI_STATUS
EFIAPI
DumpConfigData (
  IN  VOID      *Data,
  IN  UINTN     Size,
  OUT VOID      *AsciiBuffer OPTIONAL,
  OUT UINTN     *AsciiBufferSize OPTIONAL
  )
{
  return EFI_UNSUPPORTED;
}

/**
  This function is supplied with a buffer of configuration data defined according to 
  platform YAML files. Platform should update the applicable silicon policies.

  @param[in]  PolicyInterface Pointer to current policy protocol/PPI interface.
  @param[in]  Data            Pointer to platform configuration data.
  @param[in]  Size            The size of Data.

  @retval EFI_SUCCESS           The configuration is successfully applied to policy.
  @retval EFI_INVALID_PARAMETER The input data does not comply with platform configuration format,
                                or the policy interface is NULL.
  @retval EFI_UNSUPPORTED       This function is not supported for this instance.
  @retval Others                Other errors occurred.
**/
EFI_STATUS
EFIAPI
ProcessIncomingConfigData (
  IN POLICY_PROTOCOL  PolicyInterface,
  IN VOID             *Data,
  IN UINTN            Size
  )
{
  return EFI_UNSUPPORTED;
}
