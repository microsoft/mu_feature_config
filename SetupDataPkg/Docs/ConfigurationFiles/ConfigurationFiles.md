# Configuration Files Specification

## Table of Contents

- [Description](#description)
- [Revision History](#revision-history)
- [Terms](#terms)
- [Introduction](#introduction)
- [Specification Differences](#specification-differences)

## Description

This document is intended to describe the Project MU version of Configuration Files Specification.

## Revision History

| Revised by   | Date      | Changes           |
| ------------ | --------- | ------------------|
| Kun Qin   | 11/29/2021| First draft |
| Oliver Smith-Denny | 7/22/2022 | Add YAML/XML Merged Support |

## Terms

| Term   | Description                     |
| ------ | ------------------------------- |
| UEFI | Unified Extensible Firmware Interface |

## Reference Documents

| Document                                  | Link                                |
| ----------------------------------------- | ----------------------------------- |
| Slim Bootloader Repo | <https://github.com/slimbootloader/slimbootloader> |
| Configuration YAML Spec | <https://slimbootloader.github.io/specs/config.html#configuration-description-yaml-explained> |
| Project Mu Document | <https://microsoft.github.io/mu/> |
| Configuration Apps Repo | <https://windowspartners.visualstudio.com/MSCoreUEFI/_git/mu_config_apps> |

## Introduction

As Project MU inherits tool set from Slim Bootloader repository to support setup variable feature, certain modifications
has been made to improve workflow and architectural abstraction.

Although the syntax of configuration YAML files mainly follow Slim Bootloader specification for design simplicity, this
document mainly listed the differences between Project MU version and original Slim Bootloader. Additionally, this document
describes the XML format that will be accepted.

## YAML Specification Differences

- All UI related form fields (i.e. `name`, `type`, `help`, etc.) should be described separately from the data definitions.

- The UI form set should be named as `*_UI` following the same directory/name of data form sets (see example below). The
intention of this separation is to allow cleaner YAML layout for platform configuration template creation while maintaining
the [ConfigEditor.py](../../Tools/ConfigEditor.py) capability of updating configuration offline.

```bash
  CfgDataDef.yaml
  CfgDataDef_UI.yaml
  | Template_USB.yaml
  | Template_USB_UI.yaml
```

- For each configuration yaml file set, the data blob header is no longer required. This will be automatically populated
by the [GenCfgData.py](../../Tools/GenCfgData.py). The total size will be rounded up to *4KB* aligned boundary by used
size.

- All `CFGHDR_TMPL` can be ignored from YAML files. Instead, use a `IdTag` to denote a normal ID tag value, or `ArrayIdTag`
to denote an array ID tag value. [GenCfgData.py](../../Tools/GenCfgData.py) will automatically populate the `CFGHDR_TMPL`
content to backend database and generate the same binary data blob.

- MU version of ConfigEditor supports 3 more option for loading from and saving to SVD (Setup Variable Data) files:
  - Save Full Config Data to SVD File (supported for YAML/XML/Merged YAML + XML):
      Saving the entire defined YAML structure into encoded binary settings format. This format is useful when many tags
      of settings need updating at once, as it will save transmission overhead when serial method is selected. But this
      will *touch* all configurations defined.
  - Save Config Changes to SVD File (Supported for YAML):
      Saving only the changed tag setting into corresponding encoded binary value. This will allow the target system to
      update only the changed tag setting (i.e. Only disable GFX controllers, and leave USB ports on the same system intact)
  - Load Config Data from SVD File (Supported for YAML/XML/Merged YAML + XML):
      Once the target system has dumped current configuration from ConfApp, the output data can be viewed in ConfigEditor
      on host system.

## XML Specification

See [sampleschema.xml](../../Tools/sampleschema.xml) for an example XML schema.

Configuration will be organized in namespaces, each consisting of various knobs. Knobs may be built of children knobs or be a leaf knob.

Supported data types are:
  - uint8_t
  - int8_t
  - uint16_t
  - int16_t
  - uint32_t
  - int32_t
  - uint64_t
  - int64_t
  - float (note that floats are imprecise, doubles are recommend to avoid rounding errors)
  - double
  - bool

## Merged YAML + XML Operations

One YAML and one XML file may be loaded at the same time via the CLI as such:

  ```bash
  python ConfigEditor.py sampleschema.xml samplecfg.yaml
  ```
When saving config changes to delta files, two files will be output: a .dlt file for the YAML
config changes and a .csv file for the XML config changes. Either or both of these can be later
loaded to modify the current config viewed in the ConfigEditor.

As noted above under YAML Specification Differences, the full SVD can be saved in a merged
configuration. YAML config will be under Device.ConfigData.ConfigData and XML config will be
under Device.RuntimeData.RuntimeData.

For saving to/loading from a binary file, the merged config will create a list of UEFI variables
that will look as such:

|   XML Var 1  |
|   XML Var 2  |
|      ...     |
|   XML Var N  |
| YML Var List |

Where XML Var N looks like:

|   Name Size   |
|   Data Size   |
|      Name     |
|      GUID     |
|      Data     |
|      CRC      |

And YML Var List is:

|              Name Size              |
|              Data Size              |
|      DFCI_SETTINGS_REQUEST_NAME     |
|      DFCI_SETTINGS_REQUEST_GUID     |
|     Base64 Encoded YAML Only SVD    |
|                 CRC                 |


Where the Base64 Encoded YAML Only SVD is the Full SVD produced by the ConfigEditor app, containing
only the YAML configurations, and then base64 encoded, as this is the format DFCI expects.

The save to/load from raw bin option is for YAML only configurations and will save the raw binary
as is used by the build process.

The SVD is intended for use with the UEFI [Conf App](../../ConfApp/), which can take the SVD as input
and give an SVD describing the current UEFI settings as an output. The SVD is formatted to be compatible
with [DFCI](https://github.com/microsoft/mu_plus/tree/release/202202/DfciPkg).

The variable list output is intended for use with runtime configuration, to be consumed directly by
components that deal with runtime configuration.