# Software Components of the Configuration Modules

This section of documentation is focused on the software components of configuration modules that are useful during
platform integration.

`PolicyProducer` provides a software implementation that helps on sequencing the platform default silicon policy publication
and configuration data update in the PEI phase.

`ConfDataSettingProvider` is a shim layer that registers setting provider for configuration data.

The implementation of walking through configuration data is provide by platform level libraries `PlatformPolicyInitLib`,
which initializes the silicon policy for a given platform and `ConfigDataLib`, which converts configuration data to silicon
policy and serialize them into printable strings.

`ConfApp` is a UEFI application that replaces traditional UI application to display basic system information and provide
minimal functionalities, including updating system configuration data.

For more general background about the steps necessary to integrate the configuration modules, please review the
[Platform Integration Steps](PlatformIntegrationSteps.md).

## PEI Drivers

| Driver | Location |
| ---| ---|
| PolicyProducer | SetupDataPkg/PolicyProducer/PolicyProducer.inf |

## DXE Drivers

| Driver | Location |
| ---| ---|
| ConfDataSettingProvider | SetupDataPkg/ConfDataSettingProvider/ConfDataSettingProvider.inf |

## UEFI Applications

| Application | Location |
| ---| ---|
| ConfApp | SetupDataPkg/ConfApp/ConfApp.inf |

## Library Classes

*The platform owners will take the responsibility to develop and maintain their own library instances.*

| Library Class | Location |
| --- | ---|
| ConfigDataLib | SetupDataPkg/Include/Library/ConfigDataLib.h |
| PlatformPolicyInitLib | SetupDataPkg/Include/Library/PlatformPolicyInitLib.h |
