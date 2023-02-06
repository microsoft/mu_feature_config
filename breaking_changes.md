# Breaking Changes

---

**Change:** Removed DFCI based configuration support
**Owner:** kuqin12
**Date:** 2/06/2023
**Description:** Removed the DFCI settings provider driver and related modules. The configuration flow is now based
on XML and their automatically generated header files.
**PR:**[105](https://github.com/microsoft/mu_feature_config/pull/105)
**Integration:** To integrate this change:

- Remove any non-existent libraries/headers, including `ConfDataSettingProvider`, `ConfDfciUnsignedListInit`,
  `ConfProfileMgrDxe`, `ConfigBlobBaseLib`, `ConfigDataLib`, `ConfigVariableListLib`.
- Add `SvdXmlSettingSchemaSupportLib|SetupDataPkg/Library/SvdXmlSettingSchemaSupportLib/SvdXmlSettingSchemaSupportLib.inf`
  in the platform DSC file.

---

**Change:** Change Binary Suffix to .vl
**Owner:** os-d
**Date:** 1/31/2023
**Description:** Standardize the binary suffix output by ConfigEditor to .vl to be in line with other tools and
dmpstore. This reflects the variable list format being used by the binary output as opposed to the generic .bin suffix.
**PR:**[100](https://github.com/microsoft/mu_feature_config/pull/100)
**Integration:** To integrate this change, either manually change the suffix of existing binary files to .vl or make
the changes in the ConfigEditor UI tool and save new binary files.

---

**Change:** Add GUIDs to CSV Files and Load CSV Files Based on GUID/Name
**Owner:** os-d
**Date:** 1/18/23
**Description:** Make CSV loading in ConfigEditor more robust by no longer relying on filenames to load the CSV but
instead loading via namespace GUID and knob name. This change added GUIDs to the CSV output and no longer accepts the
older style CSV output that does not include the GUID information.
**PR:** [90](https://github.com/microsoft/mu_feature_config/pull/90)
**Integration:** To integrate this change, either manually add the GUID information to existing CSV files or load the
XML(s) with the new version of ConfigEditor, make the CSV changes in the tool and regenerate the CSVs.

---
