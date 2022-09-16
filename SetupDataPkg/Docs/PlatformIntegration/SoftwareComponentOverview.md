# Software Components of the Configuration Modules

This section of documentation is focused on the software components of configuration modules that are useful during
platform integration.

`ConfDataSettingProvider` is a shim layer that registers setting provider for configuration data. The underneath
implementation is operating on top of the per tag ID based configuration blob, either initialized from FV carried
original configuration blob or updated through ConfApp.

The implementation of walking through configuration data is expected to be provided by platform level module, which
initializes the silicon policy for a given platform and `ConfigDataLib`, which converts configuration data to silicon
policy and serialize them into printable strings.

`ConfApp` is a UEFI application that replaces traditional UI application to display basic system information and provide
minimal functionalities, including updating system configuration data.

`ConfProfileMgrDxe` is a DXE driver that validates and enforces the active configuration profile in MFCI Customer Mode.

For more general background about the steps necessary to integrate the configuration modules, please review the
[Platform Integration Steps](PlatformIntegrationSteps.md).

`ActiveProfileSelectorLib` is a library class that should be overwritten by the platform. It provides an interface to
retrieve the active configuration profile for this boot.

## DXE Drivers

| Driver | Location |
| ---| ---|
| ConfDataSettingProvider | SetupDataPkg/ConfDataSettingProvider/ConfDataSettingProvider.inf |
| ConfProfileMgrDxe | SetupDataPkg/ConfProfileMgrDxe/ConfProfileMgrDxe.inf |

## UEFI Applications

| Application | Location |
| ---| ---|
| ConfApp | SetupDataPkg/ConfApp/ConfApp.inf |

## Library Classes

| Library Class | Location |
| --- | ---|
| ConfigBlobBaseLib | SetupDataPkg/Include/Library/ConfigBlobBaseLib.h |
| ConfigDataLib | SetupDataPkg/Include/Library/ConfigDataLib.h |
| ActiveProfileSelectorLib | SetupDataPkg/Include/Library/ActiveProfileSelectorLib.h |
