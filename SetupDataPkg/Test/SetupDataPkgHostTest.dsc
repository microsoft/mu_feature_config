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
  SafeIntLib|MdePkg/Library/BaseSafeIntLib/BaseSafeIntLib.inf
  SecurityLockAuditLib|MdeModulePkg/Library/SecurityLockAuditLibNull/SecurityLockAuditLibNull.inf
  PerformanceLib|MdePkg/Library/BasePerformanceLibNull/BasePerformanceLibNull.inf
  XmlTreeLib|XmlSupportPkg/Library/XmlTreeLib/XmlTreeLib.inf
  XmlTreeQueryLib|XmlSupportPkg/Library/XmlTreeQueryLib/XmlTreeQueryLib.inf
  SvdXmlSettingSchemaSupportLib|SetupDataPkg/Library/SvdXmlSettingSchemaSupportLib/SvdXmlSettingSchemaSupportLib.inf
  SecureBootKeyStoreLib|MsCorePkg/Library/SecureBootKeyStoreLibNull/SecureBootKeyStoreLibNull.inf
  ConfigVariableListLib|SetupDataPkg/Library/ConfigVariableListLib/ConfigVariableListLib.inf
  ConfigSystemModeLib|SetupDataPkg/Test/MockLibrary/MockConfigSystemModeLib/MockConfigSystemModeLib.inf
  ConfigKnobShimLib|SetupDataPkg/Library/Examples/ConfigKnobShimExampleLib/ConfigKnobShimDxeLib/ConfigKnobShimDxeLib.inf

[Components]
  SetupDataPkg/Test/MockLibrary/MockUefiRuntimeServicesTableLib/MockUefiRuntimeServicesTableLib.inf
  SetupDataPkg/Test/MockLibrary/MockUefiBootServicesTableLib/MockUefiBootServicesTableLib.inf
  SetupDataPkg/Test/MockLibrary/MockResetSystemLib/MockResetSystemLib.inf
  SetupDataPkg/Test/MockLibrary/MockResetUtilityLib/MockResetUtilityLib.inf
  SetupDataPkg/Test/MockLibrary/MockPcdLib/MockPcdLib.inf
  SetupDataPkg/Test/MockLibrary/MockConfigSystemModeLib/MockConfigSystemModeLib.inf
  SetupDataPkg/Test/MockLibrary/MockPeiServicesLib/MockPeiServicesLib.inf

  SetupDataPkg/Library/ConfigVariableListLib/UnitTest/ConfigVariableListLibUnitTest.inf

  SetupDataPkg/Library/Examples/ConfigKnobShimExampleLib/ConfigKnobShimDxeLib/UnitTest/ConfigKnobShimDxeLibUnitTest.inf {
    <LibraryClasses>
      UefiRuntimeServicesTableLib|SetupDataPkg/Test/MockLibrary/MockUefiRuntimeServicesTableLib/MockUefiRuntimeServicesTableLib.inf
  }

  SetupDataPkg/Library/Examples/ConfigKnobShimExampleLib/ConfigKnobShimPeiLib/UnitTest/ConfigKnobShimPeiLibUnitTest.inf {
    <LibraryClasses>
      ConfigKnobShimLib|SetupDataPkg/Library/Examples/ConfigKnobShimExampleLib/ConfigKnobShimPeiLib/ConfigKnobShimPeiLib.inf
      PeiServicesLib|SetupDataPkg/Test/MockLibrary/MockPeiServicesLib/MockPeiServicesLib.inf
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
    <PcdsFixedAtBuild>
      gSetupDataPkgTokenSpaceGuid.PcdConfigurationPolicyList|{GUID("00000000-0000-0000-0000-000000000000")}
  }

  SetupDataPkg/ConfApp/UnitTest/ConfAppSecureBootUnitTest.inf {
    <LibraryClasses>
      UefiBootServicesTableLib|SetupDataPkg/Test/MockLibrary/MockUefiBootServicesTableLib/MockUefiBootServicesTableLib.inf
      UefiRuntimeServicesTableLib|SetupDataPkg/Test/MockLibrary/MockUefiRuntimeServicesTableLib/MockUefiRuntimeServicesTableLib.inf
      ResetSystemLib|SetupDataPkg/Test/MockLibrary/MockResetSystemLib/MockResetSystemLib.inf
  }
