/** @file
SvdUsb.h

SvdUsb loads SVD Configuration from a USB drive

Copyright (C) Microsoft Corporation. All rights reserved.
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef SVD_USB_H_
#define SVD_USB_H_

// MAX_USB_FILE_NAME_LENGTH Includes the terminating NULL
#define MAX_USB_FILE_NAME_LENGTH  256

/**
*
*  Request a XML SVD settings packet.
*
*  @param[in]     FileName        What file to read.
*  @param[out]    JsonString      Where to store the Json String
*  @param[out]    JsonStringSize  Size of Json String
*
*  @retval   Status               Always returns success.
*
**/
EFI_STATUS
EFIAPI
SvdRequestXmlFromUSB (
  IN  CHAR16  *FileName,
  OUT CHAR8   **JsonString,
  OUT UINTN   *JsonStringSize
  );

#endif // SVD_USB_H_
