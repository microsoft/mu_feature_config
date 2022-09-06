/** @file
  Unit tests of the ConfProfileMgrDxe module

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/ConfigVariableListLib.h>
#include <Library/ActiveProfileSelectorLib.h>

#include <Library/UnitTestLib.h>

#include "ConfProfileMgrDxeUnitTest.h"

#define UNIT_TEST_APP_NAME     "Configuration Profile Manager DXE Unit Tests"
#define UNIT_TEST_APP_VERSION  "1.0"

/**
  Mocked version of GetSectionFromAnyFv.

  @param  NameGuid             A pointer to to the FFS filename GUID to search for
                               within any of the firmware volumes in the platform.
  @param  SectionType          Indicates the FFS section type to search for within
                               the FFS file specified by NameGuid.
  @param  SectionInstance      Indicates which section instance within the FFS file
                               specified by NameGuid to retrieve.
  @param  Buffer               On output, a pointer to a callee allocated buffer
                               containing the FFS file section that was found.
                               Is it the caller's responsibility to free this buffer
                               using FreePool().
  @param  Size                 On output, a pointer to the size, in bytes, of Buffer.

  @retval  EFI_SUCCESS          The specified FFS section was returned.
  @retval  EFI_NOT_FOUND        The specified FFS section could not be found.
  @retval  EFI_OUT_OF_RESOURCES There are not enough resources available to
                                retrieve the matching FFS section.
  @retval  EFI_DEVICE_ERROR     The FFS section could not be retrieves due to a
                                device error.
  @retval  EFI_ACCESS_DENIED    The FFS section could not be retrieves because the
                                firmware volume that
                                contains the matching FFS section does not allow reads.
**/
EFI_STATUS
EFIAPI
GetSectionFromAnyFv (
  IN CONST  EFI_GUID          *NameGuid,
  IN        EFI_SECTION_TYPE  SectionType,
  IN        UINTN             SectionInstance,
  OUT       VOID              **Buffer,
  OUT       UINTN             *Size
  )
{
  VOID  *ret_buf;

  assert_int_equal (SectionType, EFI_SECTION_RAW);
  assert_non_null (Buffer);
  assert_non_null (Size);

  ret_buf = (VOID *)mock ();
  if (ret_buf != NULL) {
    *Size   = (UINTN)mock ();
    *Buffer = AllocateCopyPool (*Size, ret_buf);
  } else {
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}

/**
  Mock retrieving a PCD Ptr.

  Returns the pointer to the buffer of the token specified by TokenNumber.

  @param[in]  TokenNumber The PCD token number to retrieve a current value for.

  @return Returns the pointer to the token specified by TokenNumber.

**/
VOID *
EFIAPI
LibPcdGetPtr (
  IN UINTN  TokenNumber
  )
{
  return (VOID *)mock ();
}

/**
  Unit test for ActiveProfileSelectorNull.

  @param[in]  Context    [Optional] An optional parameter that enables:
                         1) test-case reuse with varied parameters and
                         2) test-case re-entry for Target tests that need a
                         reboot.  This parameter is a VOID* and it is the
                         responsibility of the test author to ensure that the
                         contents are well understood by all test cases that may
                         consume it.

  @retval  UNIT_TEST_PASSED             The Unit test has completed and the test
                                        case was successful.
  @retval  UNIT_TEST_ERROR_TEST_FAILED  A test case assertion has failed.
**/
UNIT_TEST_STATUS
EFIAPI
RetrieveActiveProfileGuidShouldMatch (
  IN UNIT_TEST_CONTEXT  Context
  )
{
  EFI_STATUS  Status;
  EFI_GUID    *Guid;

  will_return (LibPcdGetPtr, &gSetupDataPkgGenericProfileGuid);
  will_return (LibPcdGetPtr, { gZeroGuid, gSetupDataPkgGenericProfileGuid });

  Status = RetrieveActiveProfileGuid (&Guid);

  UT_ASSERT_NOT_EFI_ERROR (Status);
}

/**
  Initialize the unit test framework, suite, and unit tests for ConfProfileMgrDxe
  and run the unit tests.

  @retval  EFI_SUCCESS           All test cases were dispatched.
  @retval  EFI_OUT_OF_RESOURCES  There are not enough resources available to
                                 initialize the unit tests.
**/
STATIC
EFI_STATUS
EFIAPI
UnitTestingEntry (
  VOID
  )
{
  EFI_STATUS                  Status;
  UNIT_TEST_FRAMEWORK_HANDLE  Framework;
  UNIT_TEST_SUITE_HANDLE      ConfProfileMgrDxeTests;
  UNIT_TEST_SUITE_HANDLE      ActiveProfileSelectorLibNullTests;
  CONTEXT_DATA                Context;

  Framework = NULL;

  DEBUG ((DEBUG_INFO, "%a v%a\n", UNIT_TEST_APP_NAME, UNIT_TEST_APP_VERSION));

  //
  // Start setting up the test framework for running the tests.
  //
  Status = InitUnitTestFramework (&Framework, UNIT_TEST_APP_NAME, gEfiCallerBaseName, UNIT_TEST_APP_VERSION);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in InitUnitTestFramework. Status = %r\n", Status));
    goto EXIT;
  }

  //
  // Populate the ActiveProfileSelectorLib Unit Test Suite.
  //
  Status = CreateUnitTestSuite (&ActiveProfileSelectorLibNullTests, Framework, "ActiveProfileSelectorLibNull Tests", "ActiveProfileSelectorLibNull", NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in CreateUnitTestSuite for ConfProfileMgrDxeTests\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  //
  // Populate the ConfProfileMgrDxe Unit Test Suite.
  //
  Status = CreateUnitTestSuite (&ConfProfileMgrDxeTests, Framework, "ConfProfileMgrDxe Tests", "ConfProfileMgrDxe.Conf", NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed in CreateUnitTestSuite for ConfProfileMgrDxeTests\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto EXIT;
  }

  //
  // --------------Suite-----------Description--------------Name----------Function--------Pre---Post-------------------Context-----------
  //
  AddTestCase (ConfProfileMgrDxeTests, "RetrieveActiveProfileGuid should succeed when given generic profile", "RetrieveActiveProfileGuidShouldMatch", RetrieveActiveProfileGuidShouldMatch, NULL, NULL, NULL);

  //
  // Execute the tests.
  //
  Status = RunAllTestSuites (Framework);

EXIT:
  if (Framework) {
    FreeUnitTestFramework (Framework);
  }

  return Status;
}

/**
  Standard POSIX C entry point for host based unit test execution.
**/
int
main (
  int   argc,
  char  *argv[]
  )
{
  return UnitTestingEntry ();
}
