/** @file
  This is a sample to demonstrates the use of GoogleTest that supports host
  execution environments.

  Copyright (c) 2022, Microsoft Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <gtest/gtest.h>
extern "C" {
  #include <Uefi.h>
  #include <Library/BaseLib.h>
  #include <Library/DebugLib.h>
}


/**
  Sample unit test that verifies the expected result of an unsigned integer
  addition operation.
**/
TEST(SimpleMathTests, OnePlusOneShouldEqualTwo) {
  UINTN  A;
  UINTN  B;
  UINTN  C;

  A = 1;
  B = 1;
  C = A + B;

  ASSERT_EQ (C, (UINTN)2);
}

/**
  Sample unit test that verifies that a global BOOLEAN is updatable.
**/
class GlobalBooleanVarTests : public ::testing::Test {
  public:
    BOOLEAN  SampleGlobalTestBoolean  = FALSE;
};

TEST_F(GlobalBooleanVarTests, GlobalBooleanShouldBeChangeable) {
  SampleGlobalTestBoolean = TRUE;
  ASSERT_TRUE (SampleGlobalTestBoolean);

  SampleGlobalTestBoolean = FALSE;
  ASSERT_FALSE (SampleGlobalTestBoolean);
}

/**
  Sample unit test that logs a warning message and verifies that a global
  pointer is updatable.
**/
class GlobalVarTests : public ::testing::Test {
  public:
    VOID  *SampleGlobalTestPointer = NULL;
    UINT64  Result = 32;


  protected:
  void SetUp() override {
    ASSERT_EQ ((UINTN)SampleGlobalTestPointer, (UINTN)NULL);
  }
  void TearDown() {
    SampleGlobalTestPointer = NULL;
  }
};

TEST_F(GlobalVarTests, GlobalPointerShouldBeChangeable) {
  SampleGlobalTestPointer = (VOID *)-1;
  ASSERT_EQ ((UINTN)SampleGlobalTestPointer, (UINTN)((VOID *)-1));

  //
  // This test passes because the pointer is never NULL.
  //    
  ASSERT_NE (&Result, (UINT64 *)NULL);
}


/**
  Sample unit test using the SCOPED_TRACE() macro for trace messages.
**/
TEST(MacroTestsMessages, MacroTraceMessage) {
  //
  // Example of logging.
  //
  SCOPED_TRACE ("SCOPED_TRACE message\n");
}

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}