# Project Mu Configuration Repository

??? info "Git Details"
    Repository Url: {{mu_feature_config.url}}  
    Branch:         {{mu_feature_config.branch}}  
    Commit:         [{{mu_feature_config.commit}}]({{mu_feature_config.commitlink}})  
    Commit Date:    {{mu_feature_config.date}}

This MU Configuration feature repo contains the generic Config Editor tools from Intel's Slim Bootloader repo. This
code should be consumed as needed for firmware configuration update feature support.

## Repository Philosophy

Like other Project MU feature repositories, the Configuration feature repo does not strictly follow the EDKII releases,
but instead has a continuous main branch which will periodically receive cherry-picks of needed changes from EDKII. For
stable builds, release tags will be used instead to determine commit hashes at stable points in development. Release
branches may be created as needed to facilitate a specific release with needed features, but this should be avoided.

## Consuming the MU Configuration Feature Package

Since this project does not follow the release fork model, the code should be
consumed from a release hash and should be consumed as a extdep in the platform
repo. To include, create a file named feature_config_ext_dep.yaml desired release
tag hash. This could be in the root of the project or in a subdirectory as
desired.

```yaml
{
  "scope": "global",
  "type": "git",
  "name": "FEATURE_CONFIG",
  "var_name": "FEATURE_CONFIG_PATH",
  "source": "https://github.com/microsoft/mu_feature_config.git",
  "version": "<RELEASE HASH>",
  "flags": ["set_build_var"]
}
```

Setting the the var_name and the set_build_var flags will allow the build scripts
to reference the extdep location. To make sure that the package is discoverable
for the build, the following line should also be added to the build
configurations GetPackagesPath list.

```python
shell_environment.GetBuildVars().GetValue("FEATURE_CONFIG_PATH", "")
```

*Note: If using pytool extensions older then version 0.17.0 you will need to
append the root path to the build variable string.*

After this the package should be discoverable to can be used in the build like
any other dependency.

## Code of Conduct

This project has adopted the Microsoft Open Source Code of Conduct https://opensource.microsoft.com/codeofconduct/

For more information see the Code of Conduct FAQ https://opensource.microsoft.com/codeofconduct/faq/
or contact `opencode@microsoft.com <mailto:opencode@microsoft.com>`_. with any additional questions or comments.

## Contributions

Contributions are always welcome and encouraged!
Please open any issues in the Project Mu GitHub tracker and read https://microsoft.github.io/mu/How/contributing/

* [Code Requirements](https://microsoft.github.io/mu/CodeDevelopment/requirements/)
* [Doc Requirements](https://microsoft.github.io/mu/DeveloperDocs/requirements/)

## Issues

Please open any issues in the Project Mu GitHub tracker. [More
Details](https://microsoft.github.io/mu/How/contributing/)

## Builds

Please follow the steps in the Project Mu docs to build for CI and local
testing. [More Details](https://microsoft.github.io/mu/CodeDevelopment/compile/)

## Copyright

Copyright (C) Microsoft Corporation. All rights reserved.
SPDX-License-Identifier: BSD-2-Clause-Patent
