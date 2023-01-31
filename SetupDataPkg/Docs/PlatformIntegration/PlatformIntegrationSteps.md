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

1. [Platform Data Consumption](#platform-data-consumption) - Description of expected platform workflow on how to consume
data from configuration variables convert to policy data.

1. [Configuration App Code Integration](#configuration-app-code-integration) - How to best integrate the `SetupDataPkg`
collateral into a platform firmware.

1. [Profiles Integration](#profiles-integration) - How to integrate Configuration Profiles into platform firmware.

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

The default policy should be initialized by platform module during early boot phase policy framework is ready.

After initial policy data was populated, platform logic should look up corresponding configuration data, if available,
from variable storage for the most up-to-date configuration data. Silicon policies defined in [Silicon Code Changes](#silicon-code-changes)
should then be overridden after platform translation.

- Note: If there is no configuration variable found, the module will attempt to locate default configuration data from UEFI
firmware volume blob.

During the rest of boot process, the silicon drivers will consume the updated policy data to configure hardware components.

With configuration data, config data library can walk through the configuration blob into and dispatch the data with
tag based buffer.

During DXE phase, `ConfDataSettingProvider.inf` will be loaded and register one setting provider for receiving full
configuration data (designed to reduce configuration transmission overhead), as well as an individual setting provider
per define Tag ID, based on holistic Config data blob carried in FV, in the DFCI framework. The holistic settings *setter*
will walk through the incoming binary data blob and dispatch to individual setting providers, whereas the individual
settings providers will directly operate on the `SINGLE_SETTING_PROVIDER_TEMPLATE` formatted UEFI variables for updating,
retrieving.

## Platform Data Consumption

In order for drivers provided by this package to function as expected, the platform owners are suggested for authoring
the following routines to properly consume configuration data:

### Platform Policy Initialization

Per silicon policy defintion, platforms are responsible for initializing the silicon policy with a default value when
configuration when under the circumstance that its corresponding configuration is not present or not even defined.

### YAML Config Data Translation

If the corresponding YAML configuration is defined and exposed, the platform developer should query variable with the format
of `Device.ConfigData.TagID_%08X` or defined as `SINGLE_SETTING_PROVIDER_TEMPLATE`, where the %08X should be populated
with the intended Tag ID defined in the configuration YAML file set.

### XML Platform Config Knob Shim Library

If following the XML defined configuration, the platform must author a platform level ConfigKnobShimLib that implements
the generic get knob function from the autogenerated headers from KnobService.py. An example is provided [here](../../Library/Examples/ConfigKnobShimExampleLib/).
This example library is not intended to be used directly, but as a reference for the platform to implement.

The platform must define `CONF_AUTOGEN_INCLUDE_PATH` in PlatformBuild.py. This is the absolute path that the autogenerated
header files will be placed under as such: `CONF_AUTOGEN_INCLUDE_PATH\Generated\Config*.h`. The UpdateConfigHdr.py
build plugin will create the `Generated` directory if it does not exist.

The platform must define `MU_SCHEMA_DIR` and `MU_SCHEMA_FILE_NAME` in PlatformBuild.py. These are the directory
containing the XML configuration file and the file name of the XML configuration file, respectively. These are split
apart to allow the CI build to discover a test schema to validate this process.

A platform configuration consumer then includes `<Generated/ConfigClientGenerated.h>` and uses the autogenerated
specific getter functions to access config knob data.

The platform implemented ConfigKnobShimLib must implement `GetKnobValue`, the prototype of which is in
`ConfigServiceGenerated.h`. ConfigKnobShimLib has the decision whether to return the autogenerated profile defaults
or to fetch overrides to these from other locations, such as variable storage. Furthermore, the platform
ConfigKnobShimLib can choose whether to implement config services as a library, linked to each module or as a callout
via PPI/Protocol to one Config Manager that maintains all platform data in one location.

The platform ConfigKnobShimLib will choose whether variable services are required for accessing config knobs or if in
the absence of variable services if the profile default will be returned instead. Likewise, the platfrom
ConfigKnobShimLib will choose whether a config knob failing a validation function (which can be defined in the KNOB_DATA
structure's Validator field) will cause an assert or returning the profile default value.

## Configuration App Code Integration

1. Ensure all submodules for the platform are based on the latest Project Mu version (e.g. "release/202208")
1. The prerequisites of this feature is Policy services and Project MU based BDS as well as DFCI features. This guideline
will omit the integration steps for these features. For more information about DFCI integration, please see [here](https://microsoft.github.io/mu/dyn/mu_plus/DfciPkg/Docs/PlatformIntegration/PlatformIntegrationOverview/).

> Note: A list of the libraries and modules made available by this package is provided in the
  [Software Component Overview](SoftwareComponentOverview.md).

### Platform DSC statements

Add the DSC sections below.

> Note: This is change is on top of Project MU based BDS and DFCI feature.

``` bash
[PcdsFixedAtBuild]
  # The GUID of SetupDataPkg/ConfApp/ConfApp.inf: E3624086-4FCD-446E-9D07-B6B913792071
  gEfiMdeModulePkgTokenSpaceGuid.PcdBootManagerMenuFile|{ 0x86, 0x40, 0x62, 0xe3, 0xcd, 0x4f, 0x6e, 0x44, 0x9d, 0x7, 0xb6, 0xb9, 0x13, 0x79, 0x20, 0x71 }

[LibraryClasses]
  ConfigBlobBaseLib         |SetupDataPkg/Library/ConfigBlobBaseLib/ConfigBlobBaseLib.inf
  ConfigDataLib             |SetupDataPkg/Library/ConfigDataLib/ConfigDataLib.inf
  ConfigVariableListLib     |SetupDataPkg/Library/ConfigVariableListLib/ConfigVariableListLib.inf

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
      JsonLiteParserLib|MsCorePkg/Library/JsonLiteParser/JsonLiteParser.inf
  }
```

Remove the DSC sections below.

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

Add the FDF sections below.

> Note: This is change is on top of Project MU based BDS and DFCI feature.

``` bash
[FV.YOUR_DXE_FV]
  INF SetupDataPkg/ConfDataSettingProvider/ConfDataSettingProvider.inf
  INF SetupDataPkg/ConfApp/ConfApp.inf
```

Remove the FDF sections below.

``` bash
[FV.YOUR_DXE_FV]
  # INF MdeModulePkg/Application/UiApp/UiApp.inf
```

## Profiles Integration

In order to use configuration profiles, the platform must include the above changes as well as include a YAML file that
contains the default values for the generic profile. If additional profiles are required, the platform must include YAML
delta files for each profile that are overrides on top of the generic profile. These are created through the
[Config Editor UI tool](../../Tools/ConfigEditor.py) by choosing `Save Config Changes to Change File` or
`Save Full Config to Change File` (more details in the
[Configuration Files doc](../ConfigurationFiles/ConfigurationFiles.md)).

### PlatformBuild.py Changes

Add or update the PlatformBuild.py environment variables below, where `DELTA_CONF_POLICY` is a semicolon delimited
list of the delta files representing additional profiles.

``` bash
def SetPlatformEnv(self):
  ...
  self.env.SetValue("YAML_CONF_FILE", self.mws.join(self.ws, "PlatformPkg", "CfgData", "CfgDataDef.yaml"), "Platform Hardcoded")
  self.env.SetValue("DELTA_CONF_POLICY", self.mws.join(self.ws, "PlatformPkg", "CfgData", "Profile1.dlt") + ";" +\
                    self.mws.join(self.ws, "PlatformPkg", "CfgData", "Profile2.dlt") + ";" +\
                    ...
                    self.mws.join(self.ws, "PlatformPkg", "CfgData", "ProfileN.dlt"), "Platform Hardcoded")
  ...
```

### Platform DEC Changes

The platform must define GUIDs for each additional profile beyond the generic profile (which has a defined GUID
in SetupDataPkg).

```bash
  [Guids]
  ...
  ## Example Profile 1 will be stored in FV under this GUID
  gPlatformPkgProfile1Guid = { SOME_GUID }
  ## Example Profile 2 will be stored in FV under this GUID
  gPlatformPkgProfile2Guid = { SOME_GUID }
  ...
   ## Example Profile N will be stored in FV under this GUID
  gPlatformPkgProfileNGuid = { SOME_GUID }
  ...
```

### Platform DSC Changes

The platform must add ActiveProfileSelectorLib (whether the null instance or a platform specific instance):

```bash
  [LibraryClasses]
    ...
    # Platform can override to non-Null Lib
    ActiveProfileSelectorLib|SetupDataPkg/Library/ActiveProfileSelectorLibNull/ActiveProfileSelectorLibNull.inf
    ...
```

The platform must build ConfProfileMgrDxe:

```bash
  [Components.X64, Components.AARCH64]
    ...
    # Profile Enforcement
    SetupDataPkg/ConfProfileMgrDxe/ConfProfileMgrDxe.inf
    ...
```

The platform must also override the below PCD:

```bash
  [PcdsFixedAtBuild]
    ...
    ## List of valid Profile GUIDs
    ## gSetupDataPkgGenericProfileGuid is defaulted to in case retrieved GUID is not in this list
    gSetupDataPkgTokenSpaceGuid.PcdConfigurationProfileList|{ GUID("SOME_GUID"), GUID("SOME_GUID"), ..., GUID("SOME_GUID") }
    ...
```

The platform can optionally override the below default value (only useful if using ActiveProfileSelectorLibNull):

```bash
  [PcdsDynamicExDefault]
    ...
    # Default this to gSetupDataPkgGenericProfileGuid
    gSetupDataPkgTokenSpaceGuid.PcdSetupConfigActiveProfileFile|{ GUID("SOME_GUID") }
    ...
```

### Platform FDF Changes

The platform must add ConfProfileMgrDxe and the profiles to the FDF for each desired profile and the generic profile.

```bash
  [FV.DXEFV]
    ...
    INF SetupDataPkg/ConfProfileMgrDxe/ConfProfileMgrDxe.inf
    ...

    FILE FREEFORM = gSetupDataPkgGenericProfileGuid {
    SECTION RAW = $(CONF_BIN_FILE_0)
    }
    FILE FREEFORM = gPlatformPkgProfile1Guid {
      SECTION RAW = $(CONF_BIN_FILE_1)
    }
    FILE FREEFORM = gPlatformPkgProfile2Guid {
      SECTION RAW = $(CONF_BIN_FILE_2)
    }
    ...
    FILE FREEFORM = gPlatformPkgProfileNGuid {
      SECTION RAW = $(CONF_BIN_FILE_N)
    }
    ...
```
