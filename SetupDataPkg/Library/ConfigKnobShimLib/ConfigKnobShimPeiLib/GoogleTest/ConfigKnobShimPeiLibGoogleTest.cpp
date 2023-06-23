/** @file ConfigKnobShimPeiLibGoogleTest.cpp
  This is a sample to demonstrates the use of GoogleTest that supports host
  execution environments.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/GoogleTestLib.h>
#include <GoogleTest/Library/MockUefiRuntimeServicesTableLib.h>
extern "C" {
#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

#include "../../ConfigKnobShimLibCommon.c" // include the c file to be able to unit test static function
}

#define CONFIG_KNOB_GUID  {0x52d39693, 0x4f64, 0x4ee6, {0x81, 0xde, 0x45, 0x89, 0x37, 0x72, 0x78, 0x55}}

using namespace testing;

///////////////////////////////////////////////////////////////////////////////
class GetConfigKnobOverrideFromVariableStorageTest : public Test
{
protected:
  MockPeiServicesLib PeiServicesMock; 
  MockReadOnlyVariable2 PpiVariableServicesMock; // mock of EFI_PEI_READ_ONLY_VARIABLE2_PPI
  PPI_STATUS  PpiStatus;
  EFI_STATUS Status;
  EFI_GUID ConfigKnobGuid;
  CHAR16 *ConfigKnobName;
  UINT64 ProfileDefaultValue;
  UINTN ProfileDefaultSize;
  UINT64 VariableData;
  UINT64 ConfigKnobData;

  //
  // Redefining the Test class's SetUp function for test fixtures.
  //
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
    PpiStatus           = { .Ppi = &peiReadOnlyVariablePpi, .Status = EFI_SUCCESS };
    PPIActual           = (VOID **)&PPIVariableServices;

  }
};

//
// Fail to find a cached config knob policy and fail to fetch config knob from
// variable storage. Then, set the profile default value.
//
TEST_F (GetConfigKnobOverrideFromVariableStorageTest, VariableStorageSmallBufferFailure) {
  //
  // expect the call to locate PPI to return success, "found" PPIVariableServices
  //
  EXPECT_CALL (
    PeiServicesMock,
    PeiServicesLocatePpi ()
    )
    .WillByDefault (
      Return (EFI_SUCCESS)
    );

  //
  // expect the call to GetVariable
  //
  EXPECT_CALL(
    PpiVariableServicesMock,
    pei_GetVariable ()
    )
    .WillOnce(
      DoAll (
        SetArgPointee<4>(sizeof (VariableData)),
        SetArgBuffer<5>(&VariableData, sizeof (VariableData)),
        Return (EFI_SUCCESS)
        )
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
  //
  // expect the call to locate PPI to return success, "found" PPIVariableServices
  //
  EXPECT_CALL (
    PeiServicesMock,
    PeiServicesLocatePpi ()
    )
    .WillByDefault (
      DoAll(
        Return (EFI_SUCCESS)
      )
    );

  //
  // expect the call to GetVaraible
  //
  EXPECT_CALL(
    PpiVariableServicesMock,
    pei_GetVariable ()
    )
    .WillOnce(
      DoAll (
        SetArgPointee<4>(sizeof (VariableData)),
        SetArgBuffer<5>(&VariableData, sizeof (VariableData)),
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
  //
  // Expect the first GetVariable call to get size to fail
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

/*
  Fail to find a cached config knob policy and succeed to fetch config knob from
  variable storage. Fail to match variable size with profile default size. Then, set the profile default value.
*/
TEST_F (GetConfigKnobOverrideFromVariableStorageTest, VariableStorageSizeFailure) {
  //
  // Expect the first GetVariable call to get (non-matching) size
  //
  EXPECT_CALL (
    PeiServicesMock,
    PeiServicesLocatePpi
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
