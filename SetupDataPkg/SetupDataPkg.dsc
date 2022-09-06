## @file
# Project MU Setup Data package CI dsc file
#
# Copyright (c) Microsoft Corporation.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
##

[Defines]
  PLATFORM_NAME                  = SetupDataPkg
  PLATFORM_GUID                  = 81076AE0-741C-4061-99E2-15931C7D0A13
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010005
  OUTPUT_DIRECTORY               = Build/SetupDataPkg
  SUPPORTED_ARCHITECTURES        = IA32|X64|ARM|AARCH64
  BUILD_TARGETS                  = DEBUG|RELEASE|NOOPT
  SKUID_IDENTIFIER               = DEFAULT

[LibraryClasses]
  BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
  DebugPrintErrorLevelLib|MdePkg/Library/BaseDebugPrintErrorLevelLib/BaseDebugPrintErrorLevelLib.inf
  DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
  HobLib|MdeModulePkg/Library/BaseHobLibNull/BaseHobLibNull.inf
  FileHandleLib|MdePkg/Library/UefiFileHandleLib/UefiFileHandleLib.inf
  UefiDriverEntryPoint|MdePkg/Library/UefiDriverEntryPoint/UefiDriverEntryPoint.inf
  DxeServicesLib|MdePkg/Library/DxeServicesLib/DxeServicesLib.inf
  RegisterFilterLib|MdePkg/Library/RegisterFilterLibNull/RegisterFilterLibNull.inf
  ResetSystemLib|MdeModulePkg/Library/BaseResetSystemLibNull/BaseResetSystemLibNull.inf
  UefiBootManagerLib|MdeModulePkg/Library/BaseUefiBootManagerLibNull/BaseUefiBootManagerLibNull.inf
  VariablePolicyHelperLib|MdeModulePkg/Library/VariablePolicyHelperLib/VariablePolicyHelperLib.inf
  PerformanceLib|MdePkg/Library/BasePerformanceLibNull/BasePerformanceLibNull.inf
  SafeIntLib|MdePkg/Library/BaseSafeIntLib/BaseSafeIntLib.inf
  ResetUtilityLib|MdeModulePkg/Library/ResetUtilityLib/ResetUtilityLib.inf

  XmlTreeLib|XmlSupportPkg/Library/XmlTreeLib/XmlTreeLib.inf
  XmlTreeQueryLib|XmlSupportPkg/Library/XmlTreeQueryLib/XmlTreeQueryLib.inf
  JsonLiteParserLib|MsCorePkg/Library/JsonLiteParser/JsonLiteParser.inf
  MuUefiVersionLib|PcBdsPkg/Library/MuUefiVersionLibNull/MuUefiVersionLibNull.inf

  DfciXmlSettingSchemaSupportLib|DfciPkg/Library/DfciXmlSettingSchemaSupportLib/DfciXmlSettingSchemaSupportLib.inf
  DfciUiSupportLib|DfciPkg/Library/DfciUiSupportLibNull/DfciUiSupportLibNull.inf
  DfciDeviceIdSupportLib|DfciPkg/Library/DfciDeviceIdSupportLibNull/DfciDeviceIdSupportLibNull.inf
  DfciV1SupportLib|DfciPkg/Library/DfciV1SupportLibNull/DfciV1SupportLibNull.inf
  ConfigDataLib|SetupDataPkg/Library/ConfigDataLib/ConfigDataLib.inf
  ConfigBlobBaseLib|SetupDataPkg/Library/ConfigBlobBaseLib/ConfigBlobBaseLib.inf
  ConfigVariableListLib|SetupDataPkg/Library/ConfigVariableListLib/ConfigVariableListLib.inf
  ConfigSystemModeLib|SetupDataPkg/Library/ConfigSystemModeLibNull/ConfigSystemModeLibNull.inf
  ActiveProfileSelectorLib|SetupDataPkg/Library/ActiveProfileSelectorLibNull/ActiveProfileSelectorLibNull.inf

  SecureBootVariableLib|SecurityPkg/Library/SecureBootVariableLib/SecureBootVariableLib.inf
  PlatformPKProtectionLib|SecurityPkg/Library/PlatformPKProtectionLibVarPolicy/PlatformPKProtectionLibVarPolicy.inf
  SecureBootVariableProvisionLib|SecurityPkg/Library/SecureBootVariableProvisionLib/SecureBootVariableProvisionLib.inf
  MuSecureBootKeySelectorLib|MsCorePkg/Library/MuSecureBootKeySelectorLib/MuSecureBootKeySelectorLib.inf
  SecureBootKeyStoreLib|MsCorePkg/Library/SecureBootKeyStoreLibNull/SecureBootKeyStoreLibNull.inf

[LibraryClasses.ARM, LibraryClasses.AARCH64]
  NULL|MdePkg/Library/CompilerIntrinsicsLib/ArmCompilerIntrinsicsLib.inf        # MU_CHANGE
  NULL|MdePkg/Library/BaseStackCheckLib/BaseStackCheckLib.inf

[LibraryClasses.common.PEIM]
  PeimEntryPoint|MdePkg/Library/PeimEntryPoint/PeimEntryPoint.inf
  PeiServicesLib|MdePkg/Library/PeiServicesLib/PeiServicesLib.inf
  PeiServicesTablePointerLib|MdePkg/Library/PeiServicesTablePointerLib/PeiServicesTablePointerLib.inf
  HobLib|MdePkg/Library/PeiHobLib/PeiHobLib.inf
  MemoryAllocationLib|MdePkg/Library/PeiMemoryAllocationLib/PeiMemoryAllocationLib.inf

[LibraryClasses.common.UEFI_APPLICATION]
  UefiApplicationEntryPoint|MdePkg/Library/UefiApplicationEntryPoint/UefiApplicationEntryPoint.inf

[PcdsDynamicExDefault]
  # Set a default value for such dynamic PCD
  gDfciPkgTokenSpaceGuid.PcdUnsignedPermissionsFile|{GUID("62CF29AD-FEEE-4930-B71B-4806C787C6AA")}

[Components]
  SetupDataPkg/Library/ConfigDataLib/ConfigDataLib.inf
  SetupDataPkg/Library/ConfigBlobBaseLib/ConfigBlobBaseLib.inf
  SetupDataPkg/Library/ConfigVariableListLib/ConfigVariableListLib.inf
  SetupDataPkg/Library/ConfigVariableListLibNull/ConfigVariableListLibNull.inf
  SetupDataPkg/Library/ConfigSystemModeLibNull/ConfigSystemModeLibNull.inf
  SetupDataPkg/Library/ActiveProfileSelectorLibNull/ActiveProfileSelectorLibNull.inf
  SetupDataPkg/ConfProfileMgr/ConfProfileMgrPei/ConfProfileMgrPei.inf

  SetupDataPkg/ConfDfciUnsignedListInit/ConfDfciUnsignedListInit.inf
  
  SetupDataPkg/ConfProfileMgrDxe/ConfProfileMgrDxe.inf
  SetupDataPkg/ConfDataSettingProvider/ConfDataSettingProvider.inf

[Components.X64, Components.AARCH64]
  SetupDataPkg/ConfApp/ConfApp.inf

[BuildOptions]
  *_*_*_CC_FLAGS = -D DISABLE_NEW_DEPRECATED_INTERFACES
