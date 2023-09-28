/** @file ConfigKnobShimPeiLibGoogleTest.cpp
  This is a sample to demonstrates the use of GoogleTest that supports host
  execution environments.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/GoogleTestLib.h>
#include <GoogleTest/Library/MockUefiRuntimeServicesTableLib.h>
#include <GoogleTest/Library/MockPeiServicesLib.h>
#include <GoogleTest/Ppi/MockReadOnlyVariable2.h>

extern "C" {
#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

#include "../../ConfigKnobShimLibCommon.c"   // include the c file to be able to unit test static function
}

#define CONFIG_KNOB_GUID  {0x52d39693, 0x4f64, 0x4ee6, {0x81, 0xde, 0x45, 0x89, 0x37, 0x72, 0x78, 0x55}}

using namespace testing;

///////////////////////////////////////////////////////////////////////////////
class GetConfigKnobOverrideFromVariableStorageTest : public Test
{
protected:
MockPeiServicesLib PeiServicesMock;
MockReadOnlyVariable2 PpiVariableServicesMock;   // mock of EFI_PEI_READ_ONLY_VARIABLE2_PPI
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
// Ditch out with invalid params to initial call.
//
TEST_F (GetConfigKnobOverrideFromVariableStorageTest, InvalidParamFailure) {
  Status = GetConfigKnobOverride (NULL, ConfigKnobName, (VOID *)&ConfigKnobData, ProfileDefaultSize);
  ASSERT_EQ (Status, EFI_INVALID_PARAMETER);

  Status = GetConfigKnobOverride (&ConfigKnobGuid, NULL, (VOID *)&ConfigKnobData, ProfileDefaultSize);
  ASSERT_EQ (Status, EFI_INVALID_PARAMETER);

  Status = GetConfigKnobOverride (&ConfigKnobGuid, ConfigKnobName, NULL, ProfileDefaultSize);
  ASSERT_EQ (Status, EFI_INVALID_PARAMETER);

  Status = GetConfigKnobOverride (&ConfigKnobGuid, ConfigKnobName, (VOID *)&ConfigKnobData, 0);
  ASSERT_EQ (Status, EFI_INVALID_PARAMETER);
}

//
// Fail to find a cached config knob policy and fail to locate PPI.
// Then, set the profile default value.
//
TEST_F (GetConfigKnobOverrideFromVariableStorageTest, PpiNotFoundFailure) {
  //
  // Expect the locate PPI call to fail
  //
  EXPECT_CALL (
    PeiServicesMock,
    PeiServicesLocatePpi
    )
    .WillOnce (
       Return (EFI_NOT_FOUND)
       );

  Status = GetConfigKnobOverride (&ConfigKnobGuid, ConfigKnobName, (VOID *)&ConfigKnobData, ProfileDefaultSize);

  ASSERT_EQ (Status, EFI_NOT_FOUND);
  ASSERT_EQ (ConfigKnobData, ProfileDefaultValue);
}

//
//  Fail to find a cached config knob policy and succeed to fetch config knob from
//  variable storage. Fail to match variable size with profile default size. Then, set the profile default value.
//
TEST_F (GetConfigKnobOverrideFromVariableStorageTest, VariableStorageSizeFailure) {
  // Expect the call to LocatePpi to return success, "found" PPIVariableServices (mocked)
  EXPECT_CALL (
    PeiServicesMock,
    PeiServicesLocatePpi
    )
    .WillOnce (
       DoAll (
         SetArgPointee<3>(ByRef (PpiReadOnlyVariableServices)),
         Return (EFI_SUCCESS)
         )
       );

  // Expect the call to GetVariable to return buffer too small and an invalid buffer size
  EXPECT_CALL (
    PpiVariableServicesMock,
    GetVariable
    )
    .WillOnce (
       DoAll (
         SetArgPointee<4>(sizeof (UINT32)),
         Return (EFI_BUFFER_TOO_SMALL)
         )
       );

  Status = GetConfigKnobOverride (&ConfigKnobGuid, ConfigKnobName, (VOID *)&ConfigKnobData, ProfileDefaultSize);

  ASSERT_EQ (Status, EFI_BAD_BUFFER_SIZE);
  ASSERT_EQ (ConfigKnobData, ProfileDefaultValue);
}

//
// Fail to find a cached config knob policy and fail to fetch config knob from
// variable storage. Then, set the profile default value.
//
TEST_F (GetConfigKnobOverrideFromVariableStorageTest, VariableStorageNotFoundFailure) {
  // Expect the call to LocatePpi to return success, "found" PPIVariableServices (mocked)
  EXPECT_CALL (
    PeiServicesMock,
    PeiServicesLocatePpi
    )
    .Times (2)
    .WillRepeatedly (
       DoAll (
         SetArgPointee<3>(ByRef (PpiReadOnlyVariableServices)),
         Return (EFI_SUCCESS)
         )
       );

  // Expect the calls to GetVariable to first return buffer too small then fail to retrieve variable
  EXPECT_CALL (
    PpiVariableServicesMock,
    GetVariable
    )
    .WillOnce (
       DoAll (
         SetArgPointee<4>(sizeof (VariableData)),
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
  // Expect the call to LocatePpi to return success, "found" PPIVariableServices (mocked)
  EXPECT_CALL (
    PeiServicesMock,
    PeiServicesLocatePpi
    )
    .Times (2)
    .WillRepeatedly (
       DoAll (
         SetArgPointee<3>(ByRef (PpiReadOnlyVariableServices)),
         Return (EFI_SUCCESS)
         )
       );

  // Expect the call to GetVariable to first return buffer too small then retrieve variable successfully
  EXPECT_CALL (
    PpiVariableServicesMock,
    GetVariable
    )
    .WillOnce (
       DoAll (
         SetArgPointee<4>(sizeof (VariableData)),
         Return (EFI_BUFFER_TOO_SMALL)
         )
       )
    .WillOnce (
       DoAll (
         SetArgPointee<4>(sizeof (VariableData)), // todo why do we need to set this again?
         SetArgBuffer<5>(&VariableData, sizeof (VariableData)),
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
  InitGoogleTest (&argc, argv);
  return RUN_ALL_TESTS ();
}
