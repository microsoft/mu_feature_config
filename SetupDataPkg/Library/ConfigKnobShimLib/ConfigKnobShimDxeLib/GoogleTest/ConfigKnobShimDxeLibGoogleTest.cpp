/** @file
  This is a sample to demonstrates the use of GoogleTest that supports host
  execution environments.

  Copyright (c) 2022, Microsoft Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/GoogleTestLib.h>
#include <GoogleTest/Library/MockUefiRuntimeServicesTableLib.h>
extern "C" {
#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

// include the c file to be able to unit test static function
#include "../../ConfigKnobShimLibCommon.c"
}

#define CONFIG_KNOB_GUID  {0x52d39693, 0x4f64, 0x4ee6, {0x81, 0xde, 0x45, 0x89, 0x37, 0x72, 0x78, 0x55}}

using namespace testing;

///////////////////////////////////////////////////////////////////////////////
class GetConfigKnobOverrideFromVariableStorageTest : public Test {
protected:
MockUefiRuntimeServicesTableLib RtServicesMock;
EFI_STATUS Status;
EFI_GUID ConfigKnobGuid;
CHAR16 *ConfigKnobName;
UINT64 ProfileDefaultValue;
UINTN ProfileDefaultSize;
UINT64 VariableData;
UINT64 ConfigKnobData;

void
SetUp (
  ) override
{
  ConfigKnobGuid      = CONFIG_KNOB_GUID;
  ConfigKnobName      = (CHAR16 *)L"MyDeadBeefDelivery";
  ProfileDefaultValue = 0xDEADBEEFDEADBEEF;
  ProfileDefaultSize  = sizeof (ProfileDefaultValue);
  VariableData        = 0xBEEF7777BEEF7777;
  ConfigKnobData      = ProfileDefaultValue;
}
};

TEST_F (GetConfigKnobOverrideFromVariableStorageTest, VariableStorageFailure) {
  // expect the first GetVariable call to get size
  // expect the second call to return an EFI_DEVICE_ERROR
  EXPECT_CALL (RtServicesMock, gRT_GetVariable)
    .WillOnce (
       DoAll (
         SetArgPointee<3>(sizeof (VariableData)),
         Return (EFI_BUFFER_TOO_SMALL)
         )
       )
    .WillOnce (
       Return (EFI_NOT_FOUND)
       );

  Status = GetConfigKnobOverride (&ConfigKnobGuid, ConfigKnobName, (VOID *)&ConfigKnobData, ProfileDefaultSize);

  ASSERT_EQ (Status, EFI_NOT_FOUND);
  ASSERT_EQ (ConfigKnobData, ProfileDefaultValue);
}

TEST_F (GetConfigKnobOverrideFromVariableStorageTest, VariableStorageSuccess) {
  // expect the first GetVariable call to get size
  EXPECT_CALL (
    RtServicesMock,
    gRT_GetVariable
    )
    .WillOnce (
       DoAll (
         SetArgPointee<3>(sizeof (VariableData)),
         Return (EFI_BUFFER_TOO_SMALL)
         )
       );

  // expect the second getVariable call to update data
  // NOTE: in this case, could also simply do another .WillOnce call. BUt wanted to show a little variety
  EXPECT_CALL (
    RtServicesMock,
    gRT_GetVariable (
      _,
      _,
      _,
      _,
      NotNull ()
      )
    )
    .WillOnce (
       DoAll (
         SetArgPointee<3>(sizeof (VariableData)),
         SetArgBuffer<4>(&VariableData, sizeof (VariableData)),
         Return (EFI_SUCCESS)
         )
       );

  Status = GetConfigKnobOverride (&ConfigKnobGuid, ConfigKnobName, (VOID *)&ConfigKnobData, ProfileDefaultSize);

  ASSERT_EQ (Status, EFI_SUCCESS);
  ASSERT_EQ (VariableData, ConfigKnobData);
}

int
main (
  int   argc,
  char  *argv[]
  )
{
  testing::InitGoogleTest (&argc, argv);
  return RUN_ALL_TESTS ();
}
