/** @file ConfigKnobShimDxeLibGoogleTest.cpp
  Unit tests for the ConfigKnobShimDxeLib using GoogleTest

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/GoogleTestLib.h>
#include <GoogleTest/Library/MockUefiRuntimeServicesTableLib.h>
extern "C" {
  #include <Uefi.h>
  #include <Library/BaseLib.h>
  #include <Library/DebugLib.h>
  #include <Library/ConfigKnobShimLib.h>
}

#define CONFIG_KNOB_GUID  {0x52d39693, 0x4f64, 0x4ee6, {0x81, 0xde, 0x45, 0x89, 0x37, 0x72, 0x78, 0x55}}

using namespace testing;

///////////////////////////////////////////////////////////////////////////////
class GetConfigKnobOverrideFromVariableStorageTest : public Test
{
protected:
  StrictMock<MockUefiRuntimeServicesTableLib> RtServicesMock;
  EFI_STATUS Status;
  EFI_GUID ConfigKnobGuid;
  CHAR16 *ConfigKnobName;
  UINT64 ProfileDefaultValue;
  UINTN ProfileDefaultSize;
  UINT64 VariableData;
  UINT64 ConfigKnobData;

  // Redefining the Test class's SetUp function for test fixtures.
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

//
// Fail to find a cached config knob policy and fail to fetch config knob from
// variable storage. Then, set the profile default value.
//
TEST_F (GetConfigKnobOverrideFromVariableStorageTest, VariableStorageSmallBufferFailure) {
  // Expect the first GetVariable call to set the correct size
  // Expect the second call to return an EFI_DEVICE_ERROR
  EXPECT_CALL (
    RtServicesMock,
    gRT_GetVariable (
      Char16StrEq ((CHAR16 *)L"MyDeadBeefDelivery"), // Example of using Char16 matcher
      _,
      _,
      _,
      _
      )
    )
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

//
// With no cached config knob policy, successfully fetch config knob from
// variable storage. Then, create cached policy.
//
TEST_F (GetConfigKnobOverrideFromVariableStorageTest, VariableStorageSuccess) {
  // Expect the first GetVariable call to get size
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

  // Expect the second getVariable call to update data
  // NOTE: in this case, could also simply do another .WillOnce call. But, wanted to show a little variety
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

//
// Fail to find a cached config knob policy and fail to fetch config knob from
// variable storage. Then, set the profile default value.
//
TEST_F (GetConfigKnobOverrideFromVariableStorageTest, VariableStorageNotFoundFailure) {
  // Expect the first GetVariable call to get size to fail
  EXPECT_CALL (
    RtServicesMock,
    gRT_GetVariable
    )
    .WillOnce (
       Return (EFI_NOT_FOUND)
       );

  Status = GetConfigKnobOverride (&ConfigKnobGuid, ConfigKnobName, (VOID *)&ConfigKnobData, ProfileDefaultSize);

  ASSERT_EQ (Status, EFI_NOT_FOUND);
  ASSERT_EQ (ConfigKnobData, ProfileDefaultValue);
}

/*
  Fail to find a cached config knob policy and succeed to fetch config knob from
  variable storage. Fail to match variable size with profile default size. Then, set the profile default value.
*/
TEST_F (GetConfigKnobOverrideFromVariableStorageTest, VariableStorageSizeFailure) {
  // Expect the first GetVariable call to get (non-matching) size
  EXPECT_CALL (
    RtServicesMock,
    gRT_GetVariable
    )
    .WillOnce (
       DoAll (
         SetArgPointee<3>(sizeof (UINT32)),
         Return (EFI_BUFFER_TOO_SMALL)
         )
       );

  Status = GetConfigKnobOverride (&ConfigKnobGuid, ConfigKnobName, (VOID *)&ConfigKnobData, ProfileDefaultSize);
  ASSERT_EQ (Status, EFI_BAD_BUFFER_SIZE);
  ASSERT_EQ (ConfigKnobData, ProfileDefaultValue);
}

int
main (
  int   argc,
  char  *argv[]
  )
{
  InitGoogleTest (&argc, argv);
  return RUN_ALL_TESTS ();
}
