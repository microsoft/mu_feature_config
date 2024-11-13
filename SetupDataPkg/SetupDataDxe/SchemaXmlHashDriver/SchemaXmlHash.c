/** @file SchemaXmlHash.c
  Set config xml hash to NVRAM variable.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Base.h>
#include <ConfigStdStructDefs.h>
#include <Generated/ConfigClientGenerated.h>
#include <Generated/ConfigDataGenerated.h>
#include <Generated/ConfigProfilesGenerated.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/VariablePolicyHelperLib.h>
#include <Library/VariablePolicyLib.h>
#include <Protocol/VariablePolicy.h>

#define MAX_XML_HASH_STRING_LENGTH  32

/**
  Module entry point that register to set PlatformConfigData.xml hash variable

  @param[in] ImageHandle      The image handle.
  @param[in] SystemTable      The UEFI SystemTable.

  @retval Status              The status of the variable set operation.
**/
EFI_STATUS
EFIAPI
SchemaXmlHashDxeEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                      Status;
  EFI_GUID                        SchemaXmlHashGuid  = SCHEMA_XML_HASH_GUID;
  EDKII_VARIABLE_POLICY_PROTOCOL  *VarPolicyProtocol = NULL;

  Status = gBS->LocateProtocol (&gEdkiiVariablePolicyProtocolGuid, NULL, (VOID **)&VarPolicyProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a - Failed to locate var policy protocol (%r)\n", __func__, Status));
    return Status;
  }

  // Set variable "SCHEMA_XML_HASH" to with data SCHEMA_XML_HASH
  Status = gRT->SetVariable (
                  SCHEMA_XML_HASH_VAR_NAME,
                  &SchemaXmlHashGuid,
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                  AsciiStrnLenS (SCHEMA_XML_HASH, MAX_XML_HASH_STRING_LENGTH),
                  SCHEMA_XML_HASH
                  );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to set SCHEMA_XML_HASH variable. Status = %r\n", Status));
    return Status;
  }

  Status = RegisterBasicVariablePolicy (
             VarPolicyProtocol,
             &SchemaXmlHashGuid,
             SCHEMA_XML_HASH_VAR_NAME,
             MAX_XML_HASH_STRING_LENGTH,
             MAX_XML_HASH_STRING_LENGTH,
             EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
             EFI_VARIABLE_NON_VOLATILE,
             VARIABLE_POLICY_TYPE_LOCK_NOW
             );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to lock SCHEMA_XML_HASH. Status=%r\n", Status));
  } else {
    DEBUG ((DEBUG_INFO, "Variable SCHEMA_XML_HASH is locked\n"));
  }

  return Status;
}
