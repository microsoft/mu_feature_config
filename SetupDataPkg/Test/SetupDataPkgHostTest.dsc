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
  SecureBootKeyStoreLib|MsCorePkg/Library/SecureBootKeyStoreLibNull/SecureBootKeyStoreLibNull.inf
  ConfigVariableListLib|SetupDataPkg/Test/MockLibrary/MockConfigVariableListLib/MockConfigVariableListLib.inf
  ConfigSystemModeLib|SetupDataPkg/Test/MockLibrary/MockConfigSystemModeLib/MockConfigSystemModeLib.inf
  ActiveProfileSelectorLib|SetupDataPkg/Library/ActiveProfileSelectorLibNull/ActiveProfileSelectorLibNull.inf

[Components]
  SetupDataPkg/Test/MockLibrary/MockUefiRuntimeServicesTableLib/MockUefiRuntimeServicesTableLib.inf
  SetupDataPkg/Test/MockLibrary/MockUefiBootServicesTableLib/MockUefiBootServicesTableLib.inf
  SetupDataPkg/Test/MockLibrary/MockResetSystemLib/MockResetSystemLib.inf
  SetupDataPkg/Test/MockLibrary/MockResetUtilityLib/MockResetUtilityLib.inf
  SetupDataPkg/Test/MockLibrary/MockConfigVariableListLib/MockConfigVariableListLib.inf
  SetupDataPkg/Test/MockLibrary/MockPcdLib/MockPcdLib.inf
  SetupDataPkg/Test/MockLibrary/MockConfigVariableListLib/MockConfigVariableListLib.inf
  SetupDataPkg/Test/MockLibrary/MockConfigSystemModeLib/MockConfigSystemModeLib.inf

  SetupDataPkg/Library/ConfigDataLib/UnitTest/ConfigDataLibUnitTest.inf
  SetupDataPkg/Library/ConfigBlobBaseLib/UnitTest/ConfigBlobBaseLibUnitTest.inf
  SetupDataPkg/Library/ConfigVariableListLib/UnitTest/ConfigVariableListLibUnitTest.inf

  SetupDataPkg/ConfProfileMgrDxe/UnitTest/ConfProfileMgrDxeUnitTest.inf {
    <LibraryClasses>
      UefiBootServicesTableLib|SetupDataPkg/Test/MockLibrary/MockUefiBootServicesTableLib/MockUefiBootServicesTableLib.inf
      UefiRuntimeServicesTableLib|SetupDataPkg/Test/MockLibrary/MockUefiRuntimeServicesTableLib/MockUefiRuntimeServicesTableLib.inf
      ResetUtilityLib|SetupDataPkg/Test/MockLibrary/MockResetUtilityLib/MockResetUtilityLib.inf
      PcdLib|SetupDataPkg/Test/MockLibrary/MockPcdLib/MockPcdLib.inf
      ConfigVariableListLib|SetupDataPkg/Library/ConfigVariableListLib/ConfigVariableListLib.inf
      DxeServicesLib|MdePkg/Library/DxeServicesLib/DxeServicesLib.inf
      DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
      UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
      HobLib|MdeModulePkg/Library/BaseHobLibNull/BaseHobLibNull.inf
    <PcdsFixedAtBuild>
      gSetupDataPkgTokenSpaceGuid.PcdConfigurationProfileList|{GUID("8464A6FF-A984-4899-A375-3DC1DB3D4227")}
  }

  SetupDataPkg/ConfDataSettingProvider/UnitTest/ConfDataSettingProviderUnitTest.inf {
    <LibraryClasses>
      UefiBootServicesTableLib|SetupDataPkg/Test/MockLibrary/MockUefiBootServicesTableLib/MockUefiBootServicesTableLib.inf
      UefiRuntimeServicesTableLib|SetupDataPkg/Test/MockLibrary/MockUefiRuntimeServicesTableLib/MockUefiRuntimeServicesTableLib.inf
  }

  SetupDataPkg/ConfApp/UnitTest/ConfAppUnitTest.inf {
    <LibraryClasses>
      UefiBootServicesTableLib|SetupDataPkg/Test/MockLibrary/MockUefiBootServicesTableLib/MockUefiBootServicesTableLib.inf
      UefiRuntimeServicesTableLib|SetupDataPkg/Test/MockLibrary/MockUefiRuntimeServicesTableLib/MockUefiRuntimeServicesTableLib.inf
      ResetSystemLib|SetupDataPkg/Test/MockLibrary/MockResetSystemLib/MockResetSystemLib.inf
  }

  SetupDataPkg/ConfApp/UnitTest/ConfAppSysInfoUnitTest.inf {
    <LibraryClasses>
      UefiBootServicesTableLib|SetupDataPkg/Test/MockLibrary/MockUefiBootServicesTableLib/MockUefiBootServicesTableLib.inf
      UefiRuntimeServicesTableLib|SetupDataPkg/Test/MockLibrary/MockUefiRuntimeServicesTableLib/MockUefiRuntimeServicesTableLib.inf
      ResetSystemLib|SetupDataPkg/Test/MockLibrary/MockResetSystemLib/MockResetSystemLib.inf
  }

  SetupDataPkg/ConfApp/UnitTest/ConfAppBootOptionUnitTest.inf {
    <LibraryClasses>
      UefiBootServicesTableLib|SetupDataPkg/Test/MockLibrary/MockUefiBootServicesTableLib/MockUefiBootServicesTableLib.inf
      UefiRuntimeServicesTableLib|SetupDataPkg/Test/MockLibrary/MockUefiRuntimeServicesTableLib/MockUefiRuntimeServicesTableLib.inf
      ResetSystemLib|SetupDataPkg/Test/MockLibrary/MockResetSystemLib/MockResetSystemLib.inf
  }

  SetupDataPkg/ConfApp/UnitTest/ConfAppSetupConfUnitTest.inf {
    <LibraryClasses>
      UefiBootServicesTableLib|SetupDataPkg/Test/MockLibrary/MockUefiBootServicesTableLib/MockUefiBootServicesTableLib.inf
      UefiRuntimeServicesTableLib|SetupDataPkg/Test/MockLibrary/MockUefiRuntimeServicesTableLib/MockUefiRuntimeServicesTableLib.inf
      ResetSystemLib|SetupDataPkg/Test/MockLibrary/MockResetSystemLib/MockResetSystemLib.inf
  }

  SetupDataPkg/ConfApp/UnitTest/ConfAppSecureBootUnitTest.inf {
    <LibraryClasses>
      UefiBootServicesTableLib|SetupDataPkg/Test/MockLibrary/MockUefiBootServicesTableLib/MockUefiBootServicesTableLib.inf
      UefiRuntimeServicesTableLib|SetupDataPkg/Test/MockLibrary/MockUefiRuntimeServicesTableLib/MockUefiRuntimeServicesTableLib.inf
      ResetSystemLib|SetupDataPkg/Test/MockLibrary/MockResetSystemLib/MockResetSystemLib.inf
  }
