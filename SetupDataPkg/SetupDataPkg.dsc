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

  SvdXmlSettingSchemaSupportLib|SetupDataPkg/Library/SvdXmlSettingSchemaSupportLib/SvdXmlSettingSchemaSupportLib.inf
  ConfigVariableListLib|SetupDataPkg/Library/ConfigVariableListLib/ConfigVariableListLib.inf
  ConfigSystemModeLib|SetupDataPkg/Library/ConfigSystemModeLibNull/ConfigSystemModeLibNull.inf
  ActiveProfileIndexSelectorLib|SetupDataPkg/Library/ActiveProfileIndexSelectorLibNull/ActiveProfileIndexSelectorLibNull.inf

  SecureBootVariableLib|SecurityPkg/Library/SecureBootVariableLib/SecureBootVariableLib.inf
  PlatformPKProtectionLib|SecurityPkg/Library/PlatformPKProtectionLibVarPolicy/PlatformPKProtectionLibVarPolicy.inf
  SecureBootVariableProvisionLib|SecurityPkg/Library/SecureBootVariableProvisionLib/SecureBootVariableProvisionLib.inf
  MuSecureBootKeySelectorLib|MsCorePkg/Library/MuSecureBootKeySelectorLib/MuSecureBootKeySelectorLib.inf
  SecureBootKeyStoreLib|MsCorePkg/Library/SecureBootKeyStoreLibNull/SecureBootKeyStoreLibNull.inf

##MSCHANGE Begin
  BaseBinSecurityLib|MdePkg/Library/BaseBinSecurityLibNull/BaseBinSecurityLibNull.inf
!if $(TOOL_CHAIN_TAG) == VS2019 or $(TOOL_CHAIN_TAG) == VS2022
[LibraryClasses.X64, LibraryClasses.IA32]
  #if debug is enabled provide StackCookie support lib so that we can link to /GS exports on MSVC
  RngLib|MdePkg/Library/BaseRngLib/BaseRngLib.inf
[LibraryClasses.X64]
  BaseBinSecurityLib|MdePkg/Library/BaseBinSecurityLibRng/BaseBinSecurityLibRng.inf
  NULL|MdePkg/Library/BaseBinSecurityLibRng/BaseBinSecurityLibRng.inf
!endif
##MSCHANGE End

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

[Components]
  SetupDataPkg/Library/ConfigVariableListLib/ConfigVariableListLib.inf
  SetupDataPkg/Library/ConfigSystemModeLibNull/ConfigSystemModeLibNull.inf
  SetupDataPkg/Library/ConfigKnobShimLib/ConfigKnobShimPeiLib/ConfigKnobShimPeiLib.inf
  SetupDataPkg/Library/ConfigKnobShimLib/ConfigKnobShimDxeLib/ConfigKnobShimDxeLib.inf
  SetupDataPkg/Library/SvdXmlSettingSchemaSupportLib/SvdXmlSettingSchemaSupportLib.inf
  SetupDataPkg/Library/ActiveProfileIndexSelectorLibNull/ActiveProfileIndexSelectorLibNull.inf

[Components.X64, Components.AARCH64]
  SetupDataPkg/ConfApp/ConfApp.inf

[BuildOptions]
  *_*_*_CC_FLAGS = -D DISABLE_NEW_DEPRECATED_INTERFACES
