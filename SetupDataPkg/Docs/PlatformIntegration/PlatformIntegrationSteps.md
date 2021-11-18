# Configuration Modules Platform Integration

The configuration modules source code is intended to be used with some library classes provided by platforms. In order to
integrate configuration modules into a platform firmware it is important to consider higher-level integration challenges
specific to the platform in addition to the required code changes to integrate all of the pieces.

## High-Level Considerations

1. [Configuration Apps Changes](#configuration-apps-changes) - The setup variable feature will replace existing UI application
and switch to serial base console input/output. All agents will leverage [policy framework](https://github.com/microsoft/mu_basecore/blob/release/202108/PolicyServicePkg/README.md)
to set/get hardware configurations.

1. [Silicon Code Changes](#silicon-code-changes) - The silicon firmware may need changes for:
    1. Add depex statement for drivers to load after policy database
    1. Locate policy protocol/ppis for silicon configuration

1. [Platform Data Flow](#platform-data-flow) - Modules provided in this package for platform to load/update configuration
data.

1. [Platform API Calls](#platform-api-calls) - Description of API calls from agents in this package that are provided to
iterate platform defined configuration data.

1. [Configuration App Code Integration](#configuration-app-code-integration) - How to best integrate the `SetupDataPkg`
collateral into a platform firmware.

## Configuration Apps Changes

The Configuration Applications are based on the framework of policy services and DFCI from Project MU. Start with the fundamentals
of these 2 features from [MU_BASECORE](https://github.com/microsoft/mu_basecore/blob/release/202108/PolicyServicePkg/README.md)
and [MU_PLUS](https://microsoft.github.io/mu/dyn/mu_plus/DfciPkg/Docs/Dfci_Feature/).

Instead of rendering all available options in the UEFI front page, which is backed by HII data and complicated UI frameworks,
the configuration applications will focus on configuration data pipeline and actual functionality.

All configuration data will originate from platform default silicon policy setup, any configuration data will be applied
on top of default values. More of this data flow will be documented in [Platform Data Flow](#platform-data-flow) section.

## Silicon Code Changes

For each applicable silicon drivers that needs to be configured during UEFI operation, the silicon drivers needs to be updated
to pull silicon policy data.

For each configurable silicon feature/component/module, one needs to define a structure that contains all necessary settings.
These settings will be populated with default value during PEI phase.

When silicon module is executed, the consuming module should fetch policy data from database and configure the hardware accordingly.

## Platform Data Flow

There will be few modules provided and library classes defined for platform to define their own configuration data. More
of modules needed to this feature, please see [Configuration App Code Integration](#configuration-app-code-integration).

The default policy will be [initialized by platform](#platformpolicyinitLib) data during PEI phase in `PolicyProducer.inf`
once policy framework is ready.

Immediately after initial policy data was populated, `PolicyProducer.inf` will look up configuration data blob from variable
storage for the most up-to-date configuration data.

- Note: If there is no configuration variable found, the module will attempt to locate default configuration data from UEFI
firmware volume blob.

With configuration data, [platform implemented API](#configdatalib) will translate the configuration blob into corresponding
silicon policies defined in [Silicon Code Changes](#silicon-code-changes).

During the rest of boot process, the silicon drivers will consume the updated policy data to configure hardware components.

During DXE phase, `ConfDataSettingProvider.inf` will be loaded and register a setting provider for configuration data in
the DFCI framework. The settings *setter* will be relayed to [platform supplied interface](#configdatalib) and getter is
essentially the [platform supplied serializer interface](#configdatalib).

## Platform API Calls

In order for drivers provided by this package to function as expected, the platform owners are responsible for authoring
the following library classes:

### ConfigDataLib

| Interface | Functionality |
| ---| ---|
| DumpConfigData | This interface should intake the configuration data and serialize the binary data into printable strings. |
| ProcessIncomingConfigData | This interface should iterate through the supplied configuration data, translate the configuration data into silicon policy and update the policy. |

### PlatformPolicyInitLib

| Interface | Functionality |
| ---| ---|
| PlatformPolicyInit | This interface will initialize the silicon policy data to platform default value. |

## Configuration App Code Integration

1. Ensure all submodules for the platform are based on the latest Project Mu version (e.g. "202108")
1. The prerequisites of this feature is Policy services and Project MU based BDS as well as DFCI features. This guideline
will omit the integration steps for these features. For more information about DFCI integration, please see [here](https://microsoft.github.io/mu/dyn/mu_plus/DfciPkg/Docs/PlatformIntegration/PlatformIntegrationOverview/).

> Note: A list of the libraries and modules made available by this package is provided in the
  [Software Component Overview](SoftwareComponentOverview.md).

### Platform DSC statements

1. Add the DSC sections below.

> Note: This is change is on top of Project MU based BDS and DFCI feature.

``` bash
[PcdsFixedAtBuild]
  # The GUID of SetupDataPkg/ConfApp/ConfApp.inf: E3624086-4FCD-446E-9D07-B6B913792071
  gEfiMdeModulePkgTokenSpaceGuid.PcdBootManagerMenuFile|{ 0x86, 0x40, 0x62, 0xe3, 0xcd, 0x4f, 0x6e, 0x44, 0x9d, 0x7, 0xb6, 0xb9, 0x13, 0x79, 0x20, 0x71 }

  # The GUID for configuration setup blob, $(CONF_POLICY_GUID_BYTES) should be set during pre-build time
  gSetupDataPkgTokenSpaceGuid.PcdConfigPolicyVariableGuid|$(CONF_POLICY_GUID_BYTES)

[LibraryClasses]
  ConfigDataLib|$(MU_PLATFORM_PACKAGE)/Library/ConfigDataLib/ConfigDataLib.inf

[LibraryClasses.IA32, LibraryClasses.ARM]
  PlatformPolicyInitLib|$(MU_PLATFORM_PACKAGE)/Library/PlatformPolicyInitLib/PlatformPolicyInitLib.inf

[Components.IA32, Components.ARM]
  SetupDataPkg/PolicyProducer/PolicyProducer.inf

[Components.X64, Components.AARCH64]
  #
  # Setup variables
  #
  SetupDataPkg/ConfDataSettingProvider/ConfDataSettingProvider.inf {
    <PcdsFixedAtBuild>
      gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|0x80000000
  }
  SetupDataPkg/ConfApp/ConfApp.inf {
    <LibraryClasses>
      MsSecureBootLib|OemPkg/Library/MsSecureBootLib/MsSecureBootLib.inf
      JsonLiteParserLib|MsCorePkg/Library/JsonLiteParser/JsonLiteParser.inf
      PlatformKeyLib|OemPkg/Library/PlatformKeyLibNull/PlatformKeyLibNull.inf
  }
```

1. Remove the DSC sections below.

``` bash
[Components.X64, Components.AARCH64]
  # MdeModulePkg/Application/UiApp/UiApp.inf {
  #   <LibraryClasses>
  #     NULL|MdeModulePkg/Library/DeviceManagerUiLib/DeviceManagerUiLib.inf
  #     NULL|MdeModulePkg/Library/BootManagerUiLib/BootManagerUiLib.inf
  #     NULL|MdeModulePkg/Library/BootMaintenanceManagerUiLib/BootMaintenanceManagerUiLib.inf
  #     PcdLib|MdePkg/Library/DxePcdLib/DxePcdLib.inf
  # }
```

### Platform FDF statements

1. Add the FDF sections below.

> Note: This is change is on top of Project MU based BDS and DFCI feature.

``` bash
[FV.YOUR_PEI_FV]
  INF SetupDataPkg/PolicyProducer/PolicyProducer.inf

[FV.YOUR_DXE_FV]
  INF SetupDataPkg/ConfDataSettingProvider/ConfDataSettingProvider.inf
  INF SetupDataPkg/ConfApp/ConfApp.inf
```

1. Remove the FDF sections below.

``` bash
[FV.YOUR_DXE_FV]
  # INF MdeModulePkg/Application/UiApp/UiApp.inf
```
