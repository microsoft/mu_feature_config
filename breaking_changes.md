# mu_feature_config Breaking Changes

## 1/17/23

Make CSV loading in ConfigEditor more robust by no longer relying on filenames to load the CSV but instead
loading via namespace GUID and knob name. This change added GUIDs to the CSV output and no longer accepts the older
style CSV output that does not include the GUID information. To integrate this change, either manually add the GUID
information to existing CSV files or load the XML(s) with the new version of ConfigEditor, make the CSV changes in
the tool and regenerate the CSVs.
