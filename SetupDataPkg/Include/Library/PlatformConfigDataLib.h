/** @file
  Global variables from configuration data created by autogen header.

  Note: The data fields in this file is originated from the autogen script.
        One should not change the content in this file without pairing the
        autogen scripts.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef PLATFORM_CONFIG_DATA_LIB_H_
#define PLATFORM_CONFIG_DATA_LIB_H_

#include <Uefi.h>
#include <ConfigStdStructDefs.h>

#define SCHEMA_XML_HASH_VAR_NAME  L"SCHEMA_XML_HASH"
#define SCHEMA_XML_HASH_GUID      {0x1321e012, 0x14c5, 0x42db, { 0x8c, 0xa9, 0xe7, 0x97, 0x1d, 0x88, 0x15, 0x18 }}
#define SCHEMA_XML_HASH_LEN       33

extern CHAR8      *gSchemaXmlHash;
extern KNOB_DATA  gKnobData[];
extern UINTN      gNumKnobs;
extern PROFILE    gProfileData[];
// number of profile overrides (i.e. into gProfileData)
// this does not count the generic profile, which is not
// in gProfileData, but rather in gKnobData's defaults
extern UINTN  gNumProfiles;
extern CHAR8  *gProfileFlavorNames[];
extern UINT8  gProfileFlavorIds[];
#endif // PLATFORM_CONFIG_DATA_LIB_H_
