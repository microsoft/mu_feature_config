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
document mainly listed the differences between Project MU version and original Slim Bootloader.

## Specification Differences

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
