import argparse
import os
import re
import textwrap
import xml.etree.ElementTree as ET

from GenNCCfgData import CGenNCCfgData, get_delta_vlist


class CompareConfigs:
    def __init__(self, xml_path, default_configs_obj, modified_config_obj) -> None:
        self.xml_path = xml_path
        self.default_config_obj = default_configs_obj
        self.bin_config_obj = modified_config_obj
        self.xml_information = self._parsing_xml(self.xml_path)

    def _parsing_xml(self, xml_path):
        root = ET.parse(xml_path).getroot()
        xml_info = {}
        for section in root:
            xml_info[section.tag] = {}
            for child in section:
                if section.tag == "Knobs":
                    namespace = section.get("namespace")
                    if namespace is None:
                        raise RuntimeError("Fail to get Sub root namespace")
                    xml_info[section.tag]["guid"] = namespace.strip("{}")
                    xml_info[section.tag][child.get("type")] = child.get("name")
                else:
                    xml_info[section.tag][child.get("name")] = {}
                    index_count = 0
                    for item in child:
                        if section.tag == "Enums":
                            xml_info[section.tag][child.get("name")][item.get("name")] = item.get("value")
                        elif section.tag == "Structs":
                            xml_info[section.tag][child.get("name")][index_count] = {}
                            xml_info[section.tag][child.get("name")][index_count]["name"] = item.get("name")
                            xml_info[section.tag][child.get("name")][index_count]["value"] = item.get("default")
                            xml_info[section.tag][child.get("name")][index_count]["type"] = item.get("type")
                            xml_info[section.tag][child.get("name")][index_count][
                                "prettyname"
                            ] = item.get("prettyname", "")
                            index_count += 1
        return xml_info

    def generate_csv_rows(self, schema, full, subknobs=True):
        rows = []
        guid = None
        name_list = get_delta_vlist(schema)[0]

        rows.append(["Guid", "Knob", "Value", "Binary", "Help"])

        if subknobs:
            for subknob in schema.subknobs:
                if full or subknob.name in name_list:
                    binary = subknob.format.object_to_binary(subknob.value)
                    string_binary = " ".join(map("%2.2x".__mod__, binary))

                    if subknob.namespace != guid:
                        guid = subknob.namespace
                        guid_val = guid
                    else:
                        guid_val = "*"

                    rows.append(
                        [
                            guid_val,
                            subknob.name,
                            subknob.format.object_to_string(subknob.value),
                            string_binary,
                            subknob.help,
                        ]
                    )
        else:
            for knob in schema.knobs:
                if full or knob.name in name_list:
                    binary = knob.format.object_to_binary(knob.value)
                    string_binary = " ".join(map("%2.2x".__mod__, binary))

                    if knob.namespace != guid:
                        guid = knob.namespace
                        guid_val = guid
                    else:
                        guid_val = "*"

                    rows.append(
                        [
                            guid_val,
                            knob.name,
                            knob.format.object_to_string(knob.value),
                            string_binary,
                            knob.help,
                        ]
                    )

        return rows

    def parse_csv_string(self, s):
        s = s[1:-1] if s.startswith("{") and s.endswith("}") else s
        out, buf, depth = [], "", 0
        for c in s:
            if c == "," and depth == 0:
                out.append(buf.strip())
                buf = ""
            else:
                buf += c
                if c == "{":
                    depth += 1
                elif c == "}":
                    depth -= 1
        if buf:
            out.append(buf.strip())
        if depth != 0:
            print("Unbalanced curly braces in the input string", "stop")
            return []
        return out

    def parsing_csv_data(self, csv_rows, with_eg_token_name=False):
        bios_setting_dict = {}
        for csv_data in csv_rows[1:]:
            csv_knob_value = csv_data[1]
            csv_page_name = None
            for type_name, value_name in self.xml_information["Knobs"].items():
                if value_name == csv_knob_value:
                    csv_page_name = type_name
                    break

            if csv_page_name is None:
                continue

            bios_setting_dict[csv_page_name] = {}
            csv_string_values = self.parse_csv_string(csv_data[2])
            for index, csv_string_value in enumerate(csv_string_values):
                name = self.xml_information["Structs"][csv_page_name][index]["name"]
                if with_eg_token_name is False:
                    bios_setting_dict[csv_page_name][name] = csv_string_value
                else:
                    prettyname = self.xml_information["Structs"][csv_page_name][index]["prettyname"]
                    if prettyname == "":
                        bios_setting_dict[csv_page_name][name] = csv_string_value
                    else:
                        bios_setting_dict[csv_page_name][name] = f"{csv_string_value}|{prettyname}"

        return bios_setting_dict

    def compare_mu_setting(self):
        bin_csv_data = self.generate_csv_rows(self.bin_config_obj.schema, True, False)
        default_csv_data = self.generate_csv_rows(self.default_config_obj.schema, True, False)
        modified_csv_data = self.parsing_csv_data(bin_csv_data)
        default_csv_data = self.parsing_csv_data(default_csv_data)

        pattern = r"^\d+$|^0[xX][0-9a-fA-F]+$"

        differences_found = False
        diff_cfg_list = []
        for bios_page in default_csv_data:
            if bios_page not in modified_csv_data:
                differences_found = True
                continue

            for name, cur_value in default_csv_data[bios_page].items():
                if name not in modified_csv_data[bios_page]:
                    print(f"[Missing Setting] Page: {bios_page}, Knob: {name} not found in target CSV")
                    differences_found = True
                    continue

                target_value = modified_csv_data[bios_page][name]

                if re.match(pattern, cur_value) and re.match(pattern, target_value):
                    if int(cur_value, 0) != int(target_value, 0):
                        diff_cfg_list.append(f"Modified {bios_page}.{name} from {cur_value} to {target_value} !")
                        differences_found = True
                else:
                    if cur_value != target_value:
                        diff_cfg_list.append(f"Modified {bios_page}.{name} from {cur_value} to {target_value} !")
                        differences_found = True

        if not differences_found:
            diff_cfg_list.append("✅ No Modified knobs")

        return diff_cfg_list


def load_bin_file_configs(bin_file, bin_configs_obj):
    with open(bin_file, "rb") as fd:
        new_data = bytearray(fd.read())
    bin_configs_obj.load_default_from_bin(new_data, True)


def load_xml_file_configs(flavor, xml_folder, default_configs_obj, flavor_csv_path=None):
    if flavor == "GN":
        return

    try:
        if flavor_csv_path:
            default_csv_file_path = os.path.abspath(flavor_csv_path)
            if not os.path.isfile(default_csv_file_path):
                raise RuntimeError(f"Flavor CSV file not found: {default_csv_file_path}")
        else:
            files = os.listdir(xml_folder)
            csv_file = ""
            for file in files:
                if flavor in file and file.lower().endswith(".csv"):
                    csv_file = file
                    break
            if not csv_file:
                raise RuntimeError(
                    f"Failed to find {flavor} flavor CSV in folder: {xml_folder}. "
                    "Use --flavor-csv to provide it explicitly."
                )
            default_csv_file_path = os.path.join(xml_folder, csv_file)

        default_configs_obj.override_default_value(default_csv_file_path)
    except Exception as e:
        raise RuntimeError("LOADING ERROR", str(e))


def parse_args():
    parser = argparse.ArgumentParser(
        prog="KnobDelta.py",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description="Compare a .vl config against defaults from a base XML config.",
        epilog=textwrap.dedent(
            """\
            Examples:
              python KnobDelta.py --bin data.vl --xml-path C:\\path\\config.xml
                            python KnobDelta.py -v data.vl -p C:\\path\\config.xml
                            -f <FlavorName>(e.g. GN)
                            --flavor-csv C:\\path\\profile.csv
                            python KnobDelta.py -v data.vl -p C:\\path\\config.xml
                            -f <FlavorName>(e.g. GN) -c C:\\path\\profile.csv

            Flavor behavior:
              Base BIOS flavors use XML defaults directly.
                            Non-Base BIOS flavors can use --flavor-csv explicitly.
                            Otherwise the tool searches the XML folder.
            """
        ),
    )

    parser.add_argument(
        "-v",
        "--bin",
        required=True,
        metavar="FILE",
        help="Path to .vl config file (example: data.vl).",
    )
    parser.add_argument(
        "-p",
        "--xml-path",
        "--xml_path",
        required=True,
        metavar="FILE",
        help="Path to base XML config used as schema/default source.",
    )
    parser.add_argument(
        "-f",
        "--flavor",
        default="GN",
        metavar="NAME",
        help="Flavor name (default: GN).",
    )
    parser.add_argument(
        "-c",
        "--flavor-csv",
        default=None,
        metavar="FILE",
        help="Optional path to flavor CSV (recommended for non-Base BIOS flavors).",
    )
    parser.add_argument(
        "flavor_csv_positional",
        nargs="?",
        default=None,
        metavar="PROFILE_CSV",
        help="Optional trailing flavor CSV path (same as --flavor-csv).",
    )

    args = parser.parse_args()

    if args.flavor_csv is None and args.flavor_csv_positional is not None:
        args.flavor_csv = args.flavor_csv_positional

    if not os.path.isfile(args.bin):
        parser.error(f"Binary file not found: {args.bin}")
    if not os.path.isfile(args.xml_path):
        parser.error(f"XML file not found: {args.xml_path}")
    if args.flavor.upper() != "GN" and args.flavor_csv and not os.path.isfile(args.flavor_csv):
        parser.error(f"Flavor CSV file not found: {args.flavor_csv}")

    return args


if __name__ == "__main__":
    args = parse_args()

    xml_path = os.path.abspath(args.xml_path)
    xml_folder = os.path.dirname(xml_path)
    flavor = args.flavor.upper()

    default_configs_obj = CGenNCCfgData(xml_path)
    bin_configs_obj = CGenNCCfgData(xml_path)

    load_bin_file_configs(args.bin, bin_configs_obj)
    load_xml_file_configs(flavor, xml_folder, default_configs_obj, args.flavor_csv)

    comp_results = CompareConfigs(xml_path, default_configs_obj, bin_configs_obj)
    diff_config_knobs = comp_results.compare_mu_setting()
    for knob in diff_config_knobs:
        print(knob)
