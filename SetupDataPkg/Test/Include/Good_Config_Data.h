/** @file
  Known Good Config data for unit test purpose.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef GOOD_CONFIG_DATA_H_
#define GOOD_CONFIG_DATA_H_

#define KNOWN_GOOD_XML  "<?xml version=\"1.0\" encoding=\"utf-8\"?><SettingsPacket xmlns=\"urn:UefiSettings-Schema\"><CreatedBy>Dfci Testcase Libraries</CreatedBy><CreatedOn>2022-04-29 17:19</CreatedOn><Version>1</Version><LowestSupportedVersion>1</LowestSupportedVersion><Settings><Setting><Id>Device.ConfigData.ConfigData</Id><Value>Q0ZHRBAAAACkAAAAABAAAA0AAA8AAAAAAAAAABEAAAcAAAAAIAAAAAAAAAANAAAoAAAAAAAAAAARAAAYAAAAAEQzIhFGMyIRIQAAIAAAAABHMyIRAQIDBBERIiIzM0REIiIREUREMzMZAAABAAAAAE8AAIAAAAAAAAAAAQAAAAAlAAAIAAAAAAAAAAAAAgAARnd1SW1hZ2UuYmluAAAAAAAAAAA=</Value></Setting></Settings></SettingsPacket>"

UINT8  mGood_Tag_0xF0[] = {
  0x00, 0x00, 0x00, 0x00,
};

UINT8  mGood_Tag_0xF0_Var_List[] = {
  0x42, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x44, 0x00, 0x65, 0x00, 0x76, 0x00, 0x69, 0x00,
  0x63, 0x00, 0x65, 0x00, 0x2E, 0x00, 0x43, 0x00, 0x6F, 0x00, 0x6E, 0x00, 0x66, 0x00, 0x69, 0x00,
  0x67, 0x00, 0x44, 0x00, 0x61, 0x00, 0x74, 0x00, 0x61, 0x00, 0x2E, 0x00, 0x54, 0x00, 0x61, 0x00,
  0x67, 0x00, 0x49, 0x00, 0x44, 0x00, 0x5F, 0x00, 0x30, 0x00, 0x30, 0x00, 0x30, 0x00, 0x30, 0x00,
  0x30, 0x00, 0x30, 0x00, 0x46, 0x00, 0x30, 0x00, 0x00, 0x00, 0x9F, 0x55, 0x64, 0x76, 0x9E, 0x82,
  0xE8, 0x48, 0xA4, 0x73, 0xF1, 0x2A, 0xDA, 0xD1, 0xDD, 0xD2, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x95, 0xF8, 0x70, 0xD7,
};

UINT8  mGood_Tag_0x70[] = {
  0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

UINT8  mGood_Tag_0x280[] = {
  0x00, 0x00, 0x00, 0x00,
};

UINT8  mGood_Tag_0x180[] = {
  0x44, 0x33, 0x22, 0x11, 0x46, 0x33, 0x22, 0x11,
};

UINT8  mGood_Tag_0x200[] = {
  0x47, 0x33, 0x22, 0x11, 0x01, 0x02, 0x03, 0x04, 0x11, 0x11, 0x22, 0x22, 0x33, 0x33, 0x44, 0x44, 0x22, 0x22, 0x11, 0x11, 0x44, 0x44, 0x33, 0x33,
};

UINT8  mGood_Tag_0x10[] = {
  0x4F, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
};

UINT8  mGood_Tag_0x80[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x46, 0x77, 0x75, 0x49, 0x6D, 0x61, 0x67, 0x65, 0x2E, 0x62, 0x69, 0x6E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/*
  The known good configuration data consist of per tag data from above
*/
UINT8  mKnown_Good_Config_Data[] = {
  0x43, 0x46, 0x47, 0x44, 0x10, 0x00, 0x00, 0x00, 0xa4, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00,
  0x0d, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x07,
  0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x28,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00,
  0x44, 0x33, 0x22, 0x11, 0x46, 0x33, 0x22, 0x11, 0x21, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00,
  0x47, 0x33, 0x22, 0x11, 0x01, 0x02, 0x03, 0x04, 0x11, 0x11, 0x22, 0x22, 0x33, 0x33, 0x44, 0x44,
  0x22, 0x22, 0x11, 0x11, 0x44, 0x44, 0x33, 0x33, 0x19, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
  0x4f, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
  0x25, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
  0x46, 0x77, 0x75, 0x49, 0x6d, 0x61, 0x67, 0x65, 0x2e, 0x62, 0x69, 0x6e, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

UINT8  mGood_Runtime_Var_0[] = {
  0x07, 0x02, 0x03, 0x04, 0x05, 0x00, 0x00, 0x00, 0x00
};

UINT8  mGood_Runtime_Var_1[] = {
  0x01, 0x02, 0x03, 0x04, 0x05, 0x01, 0x00, 0x00, 0x00
};

UINT8  mGood_Runtime_Var_2[] = {
  0x02, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x00, 0x00, 0x00, 0x00, 0x06, 0x07,
  0x08, 0x09, 0x0A, 0x01, 0x00, 0x00, 0x00
};

UINT8  mGood_Runtime_Var_3[] = {
  0x64, 0x00, 0x00, 0x00
};

UINT8  mGood_Runtime_Var_4[] = {
  0x01
};

UINT8  mGood_Runtime_Var_5[] = {
  0x4A, 0xD8, 0x12, 0x4D, 0xFB, 0x21, 0x09, 0x40
};

UINT8  mGood_Runtime_Var_6[] = {
  0xF4, 0xFD, 0xB4, 0x3F
};

UINT8  mKnown_Good_Runtime_Data[] = {
  0x1E, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x43, 0x00, 0x4F, 0x00, 0x4D, 0x00, 0x50, 0x00,
  0x4C, 0x00, 0x45, 0x00, 0x58, 0x00, 0x5F, 0x00, 0x4B, 0x00, 0x4E, 0x00, 0x4F, 0x00, 0x42, 0x00,
  0x31, 0x00, 0x61, 0x00, 0x00, 0x00, 0xFE, 0x3E, 0xD4, 0x9F, 0xB1, 0x73, 0x41, 0xED, 0x90, 0x76,
  0x35, 0x66, 0x61, 0xD4, 0x6A, 0x42, 0x07, 0x00, 0x00, 0x00, 0x07, 0x02, 0x03, 0x04, 0x05, 0x00,
  0x00, 0x00, 0x00, 0x20, 0xE0, 0x51, 0xF7, 0x1E, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x43,
  0x00, 0x4F, 0x00, 0x4D, 0x00, 0x50, 0x00, 0x4C, 0x00, 0x45, 0x00, 0x58, 0x00, 0x5F, 0x00, 0x4B,
  0x00, 0x4E, 0x00, 0x4F, 0x00, 0x42, 0x00, 0x31, 0x00, 0x62, 0x00, 0x00, 0x00, 0xFE, 0x3E, 0xD4,
  0x9F, 0xB1, 0x73, 0x41, 0xED, 0x90, 0x76, 0x35, 0x66, 0x61, 0xD4, 0x6A, 0x42, 0x07, 0x00, 0x00,
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x01, 0x00, 0x00, 0x00, 0xDA, 0x4F, 0xE0, 0x67, 0x1C, 0x00,
  0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 0x43, 0x00, 0x4F, 0x00, 0x4D, 0x00, 0x50, 0x00, 0x4C, 0x00,
  0x45, 0x00, 0x58, 0x00, 0x5F, 0x00, 0x4B, 0x00, 0x4E, 0x00, 0x4F, 0x00, 0x42, 0x00, 0x32, 0x00,
  0x00, 0x00, 0xFE, 0x3E, 0xD4, 0x9F, 0xB1, 0x73, 0x41, 0xED, 0x90, 0x76, 0x35, 0x66, 0x61, 0xD4,
  0x6A, 0x42, 0x07, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x00,
  0x00, 0x00, 0x00, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x01, 0x00, 0x00, 0x00, 0x43, 0x65, 0x1F, 0xA8,
  0x1A, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x49, 0x00, 0x4E, 0x00, 0x54, 0x00, 0x45, 0x00,
  0x47, 0x00, 0x45, 0x00, 0x52, 0x00, 0x5F, 0x00, 0x4B, 0x00, 0x4E, 0x00, 0x4F, 0x00, 0x42, 0x00,
  0x00, 0x00, 0xFE, 0x3E, 0xD4, 0x9F, 0xB1, 0x73, 0x41, 0xED, 0x90, 0x76, 0x35, 0x66, 0x61, 0xD4,
  0x6A, 0x42, 0x07, 0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x91, 0xE0, 0x52, 0x13, 0x1A, 0x00,
  0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x42, 0x00, 0x4F, 0x00, 0x4F, 0x00, 0x4C, 0x00, 0x45, 0x00,
  0x41, 0x00, 0x4E, 0x00, 0x5F, 0x00, 0x4B, 0x00, 0x4E, 0x00, 0x4F, 0x00, 0x42, 0x00, 0x00, 0x00,
  0xFE, 0x3E, 0xD4, 0x9F, 0xB1, 0x73, 0x41, 0xED, 0x90, 0x76, 0x35, 0x66, 0x61, 0xD4, 0x6A, 0x42,
  0x07, 0x00, 0x00, 0x00, 0x01, 0xEE, 0x9A, 0x3F, 0x01, 0x18, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00,
  0x00, 0x44, 0x00, 0x4F, 0x00, 0x55, 0x00, 0x42, 0x00, 0x4C, 0x00, 0x45, 0x00, 0x5F, 0x00, 0x4B,
  0x00, 0x4E, 0x00, 0x4F, 0x00, 0x42, 0x00, 0x00, 0x00, 0xFE, 0x3E, 0xD4, 0x9F, 0xB1, 0x73, 0x41,
  0xED, 0x90, 0x76, 0x35, 0x66, 0x61, 0xD4, 0x6A, 0x42, 0x07, 0x00, 0x00, 0x00, 0x4A, 0xD8, 0x12,
  0x4D, 0xFB, 0x21, 0x09, 0x40, 0xFE, 0x9C, 0x78, 0x0E, 0x16, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00,
  0x00, 0x46, 0x00, 0x4C, 0x00, 0x4F, 0x00, 0x41, 0x00, 0x54, 0x00, 0x5F, 0x00, 0x4B, 0x00, 0x4E,
  0x00, 0x4F, 0x00, 0x42, 0x00, 0x00, 0x00, 0xFE, 0x3E, 0xD4, 0x9F, 0xB1, 0x73, 0x41, 0xED, 0x90,
  0x76, 0x35, 0x66, 0x61, 0xD4, 0x6A, 0x42, 0x07, 0x00, 0x00, 0x00, 0xF4, 0xFD, 0xB4, 0x3F, 0x3A,
  0x96, 0x60, 0xF6
};

#endif // GOOD_CONFIG_DATA_H_
