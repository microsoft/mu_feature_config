/** @file
  Consumer driver to locate conf data from variable storage and print all contained entries.

  Potentially we will install DFCI settings provider here as well.

  Copyright (c) 2017 - 2020, Intel Corporation. All rights reserved.<BR>
  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Ppi/ReadOnlyVariable2.h>
#include <Ppi/Policy.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PeiServicesLib.h>
#include <Library/ConfigDataLib.h>
#include <Library/PlatformPolicyInitLib.h>

/**
  Searches config data in all published firmware Volumes and loads the first
  instance that contains config data.

  @param[in]  Buffer    Placeholder for address of config data located by this routine.
  @param[in]  DataSize  Placeholder for size of config data located by this routine.

  @retval EFI_SUCCESS   This function located MM core successfully.
  @retval Others        Errors returned by PeiServices routines.

**/
EFI_STATUS
FindDefaultConfigData (
  OUT VOID                    **Buffer,
  OUT UINTN                   *DataSize
  )
{
  EFI_STATUS                  Status;
  UINTN                       Instance;
  EFI_PEI_FV_HANDLE           VolumeHandle;
  EFI_PEI_FILE_HANDLE         FileHandle;

  Instance    = 0;
  while (TRUE) {
    //
    // Traverse all firmware volume instances
    //
    Status = PeiServicesFfsFindNextVolume (Instance, &VolumeHandle);
    //
    // If some error occurs here, then we cannot find any firmware
    // volume that may contain config data.
    //
    if (EFI_ERROR (Status)) {
      ASSERT_EFI_ERROR (Status);
      break;
    }

    //
    // Find the config data file type from the beginning in this firmware volume.
    //
    FileHandle = NULL;
    Status = PeiServicesFfsFindFileByName (PcdGetPtr (PcdConfigPolicyVariableGuid), VolumeHandle, &FileHandle);
    if (!EFI_ERROR (Status)) {
      //
      // Find config data FileHandle in this volume, then we skip other firmware volume and
      // return the FileHandle. Search Section now.
      //
      // TODO: DataSize is not updated!!!!
      Status = PeiServicesFfsFindSectionData (EFI_SECTION_RAW, FileHandle, Buffer);
      if (EFI_ERROR (Status)) {
        break;
      }
      return EFI_SUCCESS;
    }

    //
    // We cannot find config data in this firmware volume, then search the next volume.
    //
    Instance++;
  }

  return Status;
}

/**
  Module entry point that will check configuration data and publish them to policy database.

  @param FileHandle                     The image handle.
  @param PeiServices                    The PEI services table.

  @retval Status                        From internal routine or boot object, should not fail
**/
EFI_STATUS
EFIAPI
PolicyProducerEntry (
  IN EFI_PEI_FILE_HANDLE              FileHandle,
  IN CONST EFI_PEI_SERVICES           **PeiServices
  )
{
  EFI_STATUS                        Status;
  UINT8                             *ConfData = NULL;
  UINT32                            Attr = 0;
  UINTN                             DataSize = 0;
  EFI_PEI_READ_ONLY_VARIABLE2_PPI   *VarPpi = NULL;
  POLICY_PPI                        *PolPpi = NULL;

  DEBUG((DEBUG_INFO, "%a - Entry.\n", __FUNCTION__));

  // First locate policy ppi.
  Status = PeiServicesLocatePpi (&gPeiPolicyPpiGuid, 0, NULL, (VOID *)&PolPpi);
  if (EFI_ERROR (Status)){
    DEBUG ((DEBUG_ERROR, "Failed to locate Policy PPI - %r\n", Status));
    ASSERT (FALSE);
    return Status;
  }

  // Invoke the platform hook to allow them to produce the initialized policy
  Status = PlatformPolicyInit (PolPpi);
  if (EFI_ERROR (Status)){
    DEBUG ((DEBUG_ERROR, "Platform failed to publish default policy - %r\n", Status));
    ASSERT (FALSE);
    return Status;
  }

  // Then locate variable ppi.
  Status = PeiServicesLocatePpi (&gEfiPeiReadOnlyVariable2PpiGuid, 0, NULL, (VOID *)&VarPpi);
  if (EFI_ERROR (Status)){
    DEBUG ((DEBUG_ERROR, "Failed to locate EFI_PEI_READ_ONLY_VARIABLE2_PPI - %r\n", Status));
    ASSERT (FALSE);
    return Status;
  }

  DataSize = 0;
  Status = VarPpi->GetVariable (
                     VarPpi,
                     CDATA_NV_VAR_NAME,
                     PcdGetPtr (PcdConfigPolicyVariableGuid),
                     &Attr,
                     &DataSize,
                     ConfData
                     );
  if (Status == EFI_NOT_FOUND) {
    // It could be the first time, thus locate from FV
    Status = FindDefaultConfigData ((VOID **)&ConfData, &DataSize);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a Failed to locate variable blob from FV, either %r\n", __FUNCTION__, Status));
      ASSERT (FALSE);
      goto Exit;
    }
  } else if (Status == EFI_BUFFER_TOO_SMALL && Attr == CDATA_NV_VAR_ATTR) {
    ConfData = AllocatePool (DataSize);
    if (ConfData == NULL) {
      DEBUG ((DEBUG_ERROR, "%a Failed to allocate buffer for conf data (size: 0x%x)\n", __FUNCTION__, DataSize));
      Status = EFI_OUT_OF_RESOURCES;
      goto Exit;
    }

    Status = VarPpi->GetVariable (
                       VarPpi,
                       CDATA_NV_VAR_NAME,
                       PcdGetPtr(PcdConfigPolicyVariableGuid),
                       &Attr,
                       &DataSize,
                       ConfData
                       );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a Reading variable encountered some issue should not be seen... %r\n", __FUNCTION__, Status));
      ASSERT (FALSE);
      goto Exit;
    }
  } else {
    DEBUG ((DEBUG_ERROR, "%a Failed to locate conf data from variable storage - %r, attribute %x\n", __FUNCTION__, Status, Attr));
    ASSERT (FALSE);
    Status = EFI_DEVICE_ERROR;
    goto Exit;
  }

  DEBUG ((DEBUG_INFO, "%a Found the variable we are looking for. Attr: 0x%x\n", __FUNCTION__, Attr));
  DUMP_HEX (DEBUG_INFO, 0, ConfData, DataSize, "");

  // Iterate through the configuration blob
  Status = ProcessIncomingConfigData (PolPpi, ConfData, DataSize);
  ASSERT_EFI_ERROR (Status);

Exit:
  return Status;
}
