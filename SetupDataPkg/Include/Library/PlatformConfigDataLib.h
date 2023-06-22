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

extern KNOB_DATA  gKnobData[];
extern UINTN      gNumKnobs;
extern PROFILE    gProfileData[];
// number of profile overrides (i.e. into gProfileData)
// this does not count the generic profile, which is not
// in gProfileData, but rather in gKnobData's defaults
extern UINTN  gNumProfiles;
extern CHAR8  *gProfileFlavorNames[];

#endif // PLATFORM_CONFIG_DATA_LIB_H_
