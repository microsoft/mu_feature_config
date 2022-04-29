/** @file
  Unit tests of the ConfApp. Shared mock implementation of gST.

  Copyright (C) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>

EFI_STATUS
EFIAPI
MockSetAttribute (
  IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL        *This,
  IN UINTN                                  Attribute
  );

EFI_STATUS
EFIAPI
MockClearScreen (
  IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL   *This
  );

EFI_STATUS
EFIAPI
MockSetCursorPosition (
  IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL        *This,
  IN UINTN                                  Column,
  IN UINTN                                  Row
  );

EFI_STATUS
EFIAPI
MockEnableCursor (
  IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL        *This,
  IN BOOLEAN                                Visible
  );

EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL mConOut = {
  .SetAttribute = MockSetAttribute,
  .ClearScreen = MockClearScreen,
  .SetCursorPosition = MockSetCursorPosition,
  .EnableCursor = MockEnableCursor,
};

EFI_STATUS
EFIAPI
MockSetAttribute (
  IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL        *This,
  IN UINTN                                  Attribute
  )
{
  assert_ptr_equal (This, &mConOut);
  return (EFI_STATUS)mock();
}

EFI_STATUS
EFIAPI
MockClearScreen (
  IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL   *This
  )
{
  assert_ptr_equal (This, &mConOut);
  return (EFI_STATUS)mock();
}

EFI_STATUS
EFIAPI
MockSetCursorPosition (
  IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL        *This,
  IN UINTN                                  Column,
  IN UINTN                                  Row
  )
{
  assert_ptr_equal (This, &mConOut);
  check_expected (Column);
  check_expected (Row);
  return (EFI_STATUS)mock();
}

EFI_STATUS
EFIAPI
MockEnableCursor (
  IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL        *This,
  IN BOOLEAN                                Visible
  )
{
  assert_ptr_equal (This, &mConOut);
  check_expected (Visible);
  return (EFI_STATUS)mock();
}

///
/// EFI System Table
///
EFI_SYSTEM_TABLE MockSys = {
  .ConOut = &mConOut
};
