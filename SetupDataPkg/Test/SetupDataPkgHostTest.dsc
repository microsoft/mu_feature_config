## @file
# SetupDataPkg DSC file used to build host-based unit tests.
#
# Copyright (C) Microsoft Corporation.
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
##

[Defines]
  PLATFORM_NAME           = SetupDataPkgHostTest
  PLATFORM_GUID           = 429CE03F-1EAF-484E-8D69-8163ACEDAF91
  PLATFORM_VERSION        = 0.1
  DSC_SPECIFICATION       = 0x00010005
  OUTPUT_DIRECTORY        = Build/SetupDataPkg/HostTest
  SUPPORTED_ARCHITECTURES = IA32|X64
  BUILD_TARGETS           = NOOPT
  SKUID_IDENTIFIER        = DEFAULT

!include UnitTestFrameworkPkg/UnitTestFrameworkPkgHost.dsc.inc

[LibraryClasses]
  ConfigDataLib|SetupDataPkg/Library/ConfigDataLib/ConfigDataLib.inf
  ConfigBlobBaseLib|SetupDataPkg/Library/ConfigBlobBaseLib/ConfigBlobBaseLib.inf
  SafeIntLib|MdePkg/Library/BaseSafeIntLib/BaseSafeIntLib.inf
  SecurityLockAuditLib|MdeModulePkg/Library/SecurityLockAuditLibNull/SecurityLockAuditLibNull.inf
  PerformanceLib|MdePkg/Library/BasePerformanceLibNull/BasePerformanceLibNull.inf
  XmlTreeLib|XmlSupportPkg/Library/XmlTreeLib/XmlTreeLib.inf
  XmlTreeQueryLib|XmlSupportPkg/Library/XmlTreeQueryLib/XmlTreeQueryLib.inf
  DfciXmlSettingSchemaSupportLib|DfciPkg/Library/DfciXmlSettingSchemaSupportLib/DfciXmlSettingSchemaSupportLib.inf
  DfciV1SupportLib|DfciPkg/Library/DfciV1SupportLibNull/DfciV1SupportLibNull.inf
  DfciUiSupportLib|DfciPkg/Library/DfciUiSupportLibNull/DfciUiSupportLibNull.inf

[Components]
  SetupDataPkg/Test/MockLibrary/MockUefiRuntimeServicesTableLib/MockUefiRuntimeServicesTableLib.inf
  SetupDataPkg/Test/MockLibrary/MockUefiBootServicesTableLib/MockUefiBootServicesTableLib.inf
  SetupDataPkg/Test/MockLibrary/MockResetSystemLib/MockResetSystemLib.inf

  SetupDataPkg/Library/ConfigDataLib/UnitTest/ConfigDataLibUnitTest.inf
  SetupDataPkg/Library/ConfigBlobBaseLib/UnitTest/ConfigBlobBaseLibUnitTest.inf

  SetupDataPkg/ConfDataSettingProvider/UnitTest/ConfDataSettingProviderUnitTest.inf {
    <LibraryClasses>
      UefiBootServicesTableLib|SetupDataPkg/Test/MockLibrary/MockUefiBootServicesTableLib/MockUefiBootServicesTableLib.inf
      UefiRuntimeServicesTableLib|SetupDataPkg/Test/MockLibrary/MockUefiRuntimeServicesTableLib/MockUefiRuntimeServicesTableLib.inf
  }

  # SetupDataPkg/ConfApp/UnitTest/ConfAppUnitTest.inf {
  #   <LibraryClasses>
  #     UefiBootServicesTableLib|SetupDataPkg/Test/MockLibrary/MockUefiBootServicesTableLib/MockUefiBootServicesTableLib.inf
  #     ResetSystemLib|SetupDataPkg/Test/MockLibrary/MockResetSystemLib/MockResetSystemLib.inf
  # }

  # SetupDataPkg/ConfApp/UnitTest/ConfAppSysInfoUnitTest.inf {
  #   <LibraryClasses>
  #     UefiBootServicesTableLib|SetupDataPkg/Test/MockLibrary/MockUefiBootServicesTableLib/MockUefiBootServicesTableLib.inf
  #     UefiRuntimeServicesTableLib|SetupDataPkg/Test/MockLibrary/MockUefiRuntimeServicesTableLib/MockUefiRuntimeServicesTableLib.inf
  #     ResetSystemLib|SetupDataPkg/Test/MockLibrary/MockResetSystemLib/MockResetSystemLib.inf
  # }

  # SetupDataPkg/ConfApp/UnitTest/ConfAppBootOptionUnitTest.inf {
  #   <LibraryClasses>
  #     UefiBootServicesTableLib|SetupDataPkg/Test/MockLibrary/MockUefiBootServicesTableLib/MockUefiBootServicesTableLib.inf
  #     UefiRuntimeServicesTableLib|SetupDataPkg/Test/MockLibrary/MockUefiRuntimeServicesTableLib/MockUefiRuntimeServicesTableLib.inf
  #     ResetSystemLib|SetupDataPkg/Test/MockLibrary/MockResetSystemLib/MockResetSystemLib.inf
  # }

  # SetupDataPkg/ConfApp/UnitTest/ConfAppSetupConfUnitTest.inf {
  #   <LibraryClasses>
  #     UefiBootServicesTableLib|SetupDataPkg/Test/MockLibrary/MockUefiBootServicesTableLib/MockUefiBootServicesTableLib.inf
  #     UefiRuntimeServicesTableLib|SetupDataPkg/Test/MockLibrary/MockUefiRuntimeServicesTableLib/MockUefiRuntimeServicesTableLib.inf
  #     ResetSystemLib|SetupDataPkg/Test/MockLibrary/MockResetSystemLib/MockResetSystemLib.inf
  # }
