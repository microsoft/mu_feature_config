/** @file SchemaXmlHash.c
  Set config xml hash to BIOS variable

  Copyright (c) Microsoft Corporation.

**/
#include <Base.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <ConfigStdStructDefs.h>
#include <Generated/ConfigClientGenerated.h>
#include <Generated/ConfigDataGenerated.h>
#include <Generated/ConfigProfilesGenerated.h>

/**
  Module entry point that register to set PlatformConfigData.xml hash variable

  @param FileHandle                     The image handle.
  @param PeiServices                    The PEI services table.

  @retval Status                        From internal routine or boot object, should not fail
**/
EFI_STATUS
EFIAPI
SchemaXmlHashDxeEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  EFI_GUID schema_xml_hash_guid = SCHEMA_XML_HASH_GUID;

  DEBUG((DEBUG_ERROR, "In %a\n", __FUNCTION__));

  // Set variable "SCHEMA_XML_HASH" to with data SCHEMA_XML_HASH
  Status = gRT->SetVariable (
                  L"SCHEMA_XML_HASH",
                  &schema_xml_hash_guid,
                  EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                  AsciiStrLen(SCHEMA_XML_HASH),
                  SCHEMA_XML_HASH
                  );

  if (EFI_ERROR(Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to set SCHEMA_XML_HASH variable. Status = %r\n", Status));
  } else {
    DEBUG ((DEBUG_INFO, "SCHEMA_XML_HASH variable set successfully\n"));
  }

  DEBUG ((DEBUG_ERROR, "Out %a\n", __FUNCTION__));

  return Status;
}
