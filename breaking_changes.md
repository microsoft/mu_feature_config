# Breaking Changes

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
