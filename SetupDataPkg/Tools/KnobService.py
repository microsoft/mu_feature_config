# @ KnobService.py
#
# Generates C header files for a firmware component to consume UEFI variables
# as configurable knobs
#
# Copyright (c) 2022, Microsoft Corporation. All rights reserved.
#
import os
import sys
import uuid
import VariableList


def generate_public_header(schema, header_path):

    format_options = VariableList.StringFormatOptions()
    format_options.c_format = True

    with open(header_path, 'w') as out:
        out.write("#pragma once\n")
        out.write("#include <stdint.h>\n")
        out.write("#include <stddef.h>\n")
        out.write("#include <stdbool.h>\n")
        out.write("// Generated Header\n")
        out.write("//  Script: {}\n".format(sys.argv[0]))
        out.write("//  Schema: {}\n".format(schema.path))
        out.write("\n")
        out.write("#ifndef C_ASSERT\n")
        out.write("// Statically verify an expression\n")
        out.write("#define C_ASSERT(e) typedef char __C_ASSERT__[(e)?1:-1]\n")
        out.write("#endif\n")
        out.write("\n")
        out.write("#pragma pack(push, 1)")
        out.write("\n")
        out.write("// Schema-defined enums\n\n")
        for enum in schema.enums:
            if enum.help != "":
                out.write("// {}\n".format(enum.help))
            out.write("typedef enum {\n")
            has_negative = False
            for value in enum.values:
                if value.help != "":
                    out.write("    {}_{} = {}, // {}\n".format(
                        enum.name,
                        value.name,
                        value.number,
                        value.help
                    ))
                else:
                    out.write("    {}_{} = {},\n".format(
                        enum.name,
                        value.name,
                        value.number
                    ))

                if value.number < 0:
                    has_negative = True
            if has_negative:
                out.write("    _{}_PADDING = 0x7fffffff // Force packing to int size\n".format(enum.name))
            else:
                out.write("    _{}_PADDING = 0xffffffff // Force packing to int size\n".format(enum.name))
            out.write("}} {};\n".format(enum.name))
            out.write("\n")
            out.write("C_ASSERT(sizeof({}) == sizeof(uint32_t));\n".format(enum.name))
            out.write("\n")
            pass

        out.write("// Schema-defined structures\n\n")
        for struct_definition in schema.structs:
            if struct_definition.help != "":
                out.write("// {}\n".format(struct_definition.help))
            out.write("typedef struct {\n")
            for member in struct_definition.members:
                if member.help != "":
                    out.write("    // {}\n".format(member.help))
                if member.count == 1:
                    out.write("    {} {};\n".format(
                        member.format.c_type,
                        member.name))
                else:
                    out.write("    {} {}[{}];\n".format(
                        member.format.c_type,
                        member.name,
                        member.count))

            out.write("}} {};\n".format(struct_definition.name))
            out.write("\n")
            out.write("C_ASSERT(sizeof({}) == {});\n".format(struct_definition.name, struct_definition.size_in_bytes()))
            out.write("\n")
            pass

        out.write("// Schema-defined knobs\n\n")
        for knob in schema.knobs:
            out.write("// {} knob\n\n".format(knob.name))
            if knob.help != "":
                out.write("// {}\n".format(knob.help))

            out.write("\n")
            for subknob in knob.subknobs:
                if subknob.leaf:
                    define_name = subknob.name.replace('[', '_').replace(']', '_').replace('.', '__')
                    if subknob.min != subknob.format.min:
                        out.write("#define KNOB__{}__MIN {}\n".format(
                            define_name,
                            subknob.format.object_to_string(subknob.min, format_options)))
                    if subknob.max != subknob.format.max:
                        out.write("#define KNOB__{}__MAX {}\n".format(
                            define_name,
                            subknob.format.object_to_string(subknob.max, format_options)))

            out.write("\n")
            out.write("// Get the current value of the {} knob\n".format(knob.name))
            out.write("{} config_get_{}();\n".format(
                knob.format.c_type,
                knob.name))
            out.write("\n")

            out.write("#ifdef CONFIG_SET_VARIABLES\n")
            out.write("// Set the current value of the {} knob\n".format(knob.name))
            out.write("bool config_set_{}({} value);\n".format(
                knob.name,
                knob.format.c_type,
            ))
            out.write("#endif // CONFIG_SET_VARIABLES\n")
            out.write("\n")
            pass

        out.write("\n")
        out.write("typedef enum {\n")
        for knob in schema.knobs:
            out.write("    KNOB_{},\n".format(knob.name))
        out.write("    KNOB_MAX\n")
        out.write("} knob_t;\n")
        out.write("\n")
        out.write("typedef struct {\n")
        out.write("    unsigned long  Data1;\n")
        out.write("    unsigned short Data2;\n")
        out.write("    unsigned short Data3;\n")
        out.write("    unsigned char  Data4[8];\n")
        out.write("} config_guid_t;\n")
        out.write("\n")
        out.write("typedef struct {\n")
        out.write("    int get_count;\n")
        out.write("    int set_count;\n")
        out.write("} knob_statistics_t;\n")
        out.write("\n")
        out.write("typedef bool(knob_validation_fn)(const void*);")
        out.write("\n")
        out.write("typedef struct {\n")
        out.write("    knob_t knob;\n")
        out.write("    const void* default_value_address;\n")
        out.write("    void* cache_value_address;\n")
        out.write("    size_t value_size;\n")
        out.write("    const char* name;\n")
        out.write("    size_t name_size;\n")
        out.write("    config_guid_t vendor_namespace;\n")
        out.write("    int attributes;\n")
        out.write("    knob_statistics_t statistics;\n")
        out.write("    knob_validation_fn* validator;\n")
        out.write("} knob_data_t;\n")
        out.write("\n")
        out.write("typedef struct {\n")
        out.write("    knob_t knob;\n")
        out.write("    void* value;\n")
        out.write("} knob_override_t;\n")
        out.write("\n")
        out.write("typedef struct {\n")
        out.write("    knob_override_t* overrides;\n")
        out.write("    size_t override_count;\n")
        out.write("} profile_t;\n")
        out.write("\n")
        out.write("#pragma pack(pop)")
        out.write("\n")


def format_guid(guid):
    u = uuid.UUID(guid)

    byte_sequence = u.fields[3].to_bytes(1, byteorder='big') + \
                    u.fields[4].to_bytes(1, byteorder='big') + \
                    u.fields[5].to_bytes(6, byteorder='big')

    return "{{{},{},{},{{{},{},{},{},{},{},{},{}}}}}".format(
        hex(u.fields[0]),
        hex(u.fields[1]),
        hex(u.fields[2]),
        hex(byte_sequence[0]),
        hex(byte_sequence[1]),
        hex(byte_sequence[2]),
        hex(byte_sequence[3]),
        hex(byte_sequence[4]),
        hex(byte_sequence[5]),
        hex(byte_sequence[6]),
        hex(byte_sequence[7]))


def generate_cached_implementation(schema, header_path):
    with open(header_path, 'w') as out:
        out.write("#pragma once\n")
        out.write("// The config public header must be included prior to this file")
        out.write("// Generated Header\n")
        out.write("//  Script: {}\n".format(sys.argv[0]))
        out.write("//  Schema: {}\n".format(schema.path))
        out.write("\n")

        out.write("typedef struct {\n")
        for knob in schema.knobs:
            out.write("    {} {};\n".format(
                knob.format.c_type,
                knob.name))
        out.write("} knob_values_t;\n")
        out.write("\n")

        format_options = VariableList.StringFormatOptions()
        format_options.c_format = True

        out.write("const knob_values_t g_knob_default_values = {\n")
        for knob in schema.knobs:
            out.write("    .{}={},\n".format(knob.name, knob.format.object_to_string(knob.default, format_options)))
        out.write("};\n\n")

        out.write("#ifdef CONFIG_INCLUDE_CACHE\n")
        out.write("knob_values_t g_knob_cached_values = {\n")
        for knob in schema.knobs:
            out.write("    .{}={},\n".format(knob.name, knob.format.object_to_string(knob.default, format_options)))
        out.write("};\n")
        out.write("#define CONFIG_CACHE_ADDRESS(knob) (&g_knob_cached_values.knob)\n")
        out.write("#else // CONFIG_INCLUDE_CACHE\n")
        out.write("#define CONFIG_CACHE_ADDRESS(knob) (NULL)\n")
        out.write("#endif // CONFIG_INCLUDE_CACHE\n")
        out.write("\n\n")

        for enum in schema.enums:
            lowest_value = enum.values[0].number
            highest_value = enum.values[0].number
            for value in enum.values:
                if value.number < lowest_value:
                    lowest_value = value.number
                if value.number > highest_value:
                    highest_value = value.number

            out.write("bool validate_enum_value_{}({} value)\n".format(enum.name, enum.name))
            out.write("{\n")

            if highest_value - lowest_value > len(enum.values):
                out.write("    switch (value) {\n")

                for value in enum.values:
                    out.write("        case {}_{}: return true;\n".format(enum.name, value.name))
                # out.write("        default: return false;\n")
                out.write("    }\n")
                out.write("    return false;\n")
            else:
                out.write("    int numeric_value = (int)value;\n")
                out.write("    if (numeric_value < {}) return false;\n".format(lowest_value))
                out.write("    if (numeric_value > {}) return false;\n".format(highest_value))
                for i in range(lowest_value, highest_value):
                    found = False
                    for value in enum.values:
                        if value.number == i:
                            found = True
                            break
                    if not found:
                        out.write("    if (numeric_value == {}) return false;\n".format(i))
                out.write("    return true;\n")
            out.write("}\n")
            out.write("\n")

        out.write("\n")

        out.write("bool validate_knob_no_constraints(const void* buffer)\n")
        out.write("{\n")
        out.write("    (void)buffer;\n")
        out.write("    return true;\n")
        out.write("}\n")
        out.write("\n")
        for knob in schema.knobs:
            constraint_present = False
            for subknob in knob.subknobs:
                if subknob.leaf:
                    if isinstance(subknob.format, VariableList.EnumFormat):
                        constraint_present = True
                    else:
                        if subknob.min != subknob.format.min:
                            constraint_present = True
                        if subknob.max != subknob.format.max:
                            constraint_present = True

            if constraint_present:
                out.write("bool validate_knob_content_{}(const void* buffer)\n".format(knob.name))
                out.write("{\n")
                out.write("    {}* value = ({}*)buffer;\n".format(knob.format.c_type, knob.format.c_type))

                for subknob in knob.subknobs:
                    if subknob.leaf:
                        path = subknob.name[len(knob.name):]
                        define_name = subknob.name.replace('[', '_').replace(']', '_').replace('.', '__')
                        if isinstance(subknob.format, VariableList.EnumFormat):
                            out.write("    if ( !validate_enum_value_{}( (*value){} ) ) return false;\n".format(
                                subknob.format.name, path))
                        else:
                            if subknob.min != subknob.format.min:
                                out.write("    if ( (*value){} < KNOB__{}__MIN ) return false;\n".format(
                                    path,
                                    define_name))
                            if subknob.max != subknob.format.max:
                                out.write("    if ( (*value){} > KNOB__{}__MAX ) return false;\n".format(
                                    path,
                                    define_name))
                out.write("    return true;\n")
                out.write("}\n")
                out.write("\n")
            else:
                out.write("#define validate_knob_content_{} validate_knob_no_constraints\n".format(knob.name))
                out.write("\n")

        out.write("\n")
        out.write("knob_data_t g_knob_data[{}] = {{\n".format(len(schema.knobs) + 1))
        for knob in schema.knobs:
            out.write("    {\n")
            out.write("       KNOB_{},\n".format(knob.name))
            out.write("       &g_knob_default_values.{},\n".format(knob.name))
            out.write("       CONFIG_CACHE_ADDRESS({}),\n".format(knob.name))
            out.write("       sizeof({}),\n".format(knob.format.c_type))
            out.write("       \"{}\",\n".format(knob.name))
            out.write("       {}, // Length of name (including NULL terminator)\n".format(len(knob.name) + 1))
            out.write("       {}, // {}\n".format(format_guid(knob.namespace), knob.namespace))
            out.write("       {},\n".format(7))
            out.write("       {0, 0},\n")
            out.write("       &validate_knob_content_{}\n".format(knob.name))
            out.write("    },\n")
        out.write("    {\n")
        out.write("       KNOB_MAX,\n")
        out.write("       NULL,\n")
        out.write("       NULL,\n")
        out.write("       0,\n")
        out.write("       NULL,\n")
        out.write("       0,\n")
        out.write("       {0,0,0,{0,0,0,0,0,0,0,0}},\n")
        out.write("       0,\n")
        out.write("       {0, 0},\n")
        out.write("       NULL")
        out.write("    }\n")
        out.write("};\n\n")

        out.write("\n")
        out.write("void* get_knob_value(knob_t knob);\n")
        out.write("\n")
        out.write("#ifdef CONFIG_SET_VARIABLES\n")
        out.write("bool set_knob_value(knob_t knob, void* value);\n")
        out.write("#endif // CONFIG_SET_VARIABLES\n")
        out.write("\n")
        out.write("// Schema-defined knobs\n\n")
        for knob in schema.knobs:
            out.write("// {} knob\n".format(knob.name))
            if knob.help != "":
                out.write("// {}\n".format(knob.help))

            out.write("// Get the current value of the {} knob\n".format(knob.name))
            out.write("{} config_get_{}() {{\n".format(
                knob.format.c_type,
                knob.name))
            out.write("    return *(({}*)get_knob_value(KNOB_{}));\n".format(
                knob.format.c_type,
                knob.name
            ))
            out.write("}\n")
            out.write("\n")
            out.write("#ifdef CONFIG_SET_VARIABLES\n")
            out.write("// Set the current value of the {} knob\n".format(knob.name))
            out.write("bool config_set_{}({} value) {{\n".format(
                knob.name,
                knob.format.c_type))
            out.write("    return set_knob_value(KNOB_{}, &value);\n".format(
                knob.name
            ))
            out.write("}\n")
            out.write("#endif // CONFIG_SET_VARIABLES\n")
            out.write("\n")
            pass


def generate_profiles(schema, profile_header_path, profile_paths):
    with open(profile_header_path, 'w') as out:
        out.write("#pragma once\n")
        out.write("// The config public header must be included prior to this file")
        out.write("// Generated Header\n")
        out.write("//  Script: {}\n".format(sys.argv[0]))
        out.write("//  Schema: {}\n".format(schema.path))
        for profile_path in profile_paths:
            out.write("//  Profile: {}\n".format(profile_path))

        out.write("\n")

        format_options = VariableList.StringFormatOptions()
        format_options.c_format = True

        profiles = []
        for profile_path in profile_paths:
            base_name = os.path.splitext(os.path.basename(profile_path))[0]
            out.write("// Profile {}\n".format(base_name))
            # Reset the schema to defaults
            for knob in schema.knobs:
                knob.value = None

            # Read the csv to override the values in the schema
            VariableList.read_csv(schema, profile_path)

            override_count = 0

            out.write("typedef struct {\n")
            for knob in schema.knobs:
                if knob.value is not None:
                    override_count = override_count + 1
                    out.write("    {} {};\n".format(
                        knob.format.c_type,
                        knob.name))
            out.write("}} profile_{}_data_t;\n".format(base_name))

            out.write("\n")
            out.write("profile_{}_data_t profile_{}_data = {{\n".format(base_name, base_name))
            for knob in schema.knobs:
                if knob.value is not None:
                    out.write("    .{}={},\n".format(
                        knob.name,
                        knob.format.object_to_string(knob.value, format_options)))
            out.write("};\n")
            out.write("\n")
            out.write("#define PROFILE_{}_OVERRIDES\n".format(base_name.upper()))
            out.write("#define PROFILE_{}_OVERRIDES_COUNT {}\n".format(base_name.upper(), override_count))
            out.write("knob_override_t profile_{}_overrides[PROFILE_{}_OVERRIDES_COUNT + 1] = {{\n".format(
                base_name,
                base_name.upper()))

            for knob in schema.knobs:
                if knob.value is not None:
                    out.write("    {\n")
                    out.write("        .knob = KNOB_{},\n".format(knob.name))
                    out.write("        .value = &profile_{}_data.{},\n".format(base_name, knob.name))
                    out.write("    },\n")

            out.write("    {\n")
            out.write("        .knob = KNOB_MAX,\n")
            out.write("        .value = NULL,\n")
            out.write("    }\n")
            out.write("};\n")
            out.write("\n")

            profiles.append((base_name, override_count))
        out.write("\n")
        out.write("#define PROFILE_COUNT {}\n".format(len(profiles)))
        out.write("profile_t profiles[PROFILE_COUNT + 1] = {\n")
        for (profile, override_count) in profiles:
            out.write("    {\n")
            out.write("        .overrides = profile_{}_overrides,\n".format(profile))
            out.write("        .override_count = {},\n".format(override_count))
            out.write("    },\n")
        out.write("    {\n")
        out.write("        .overrides = NULL,\n")
        out.write("        .override_count = 0,\n")
        out.write("    }\n")
        out.write("};\n")


def generate_sources(schema, public_header, service_header):
    generate_public_header(schema, public_header)
    generate_cached_implementation(schema, service_header)


def usage():
    print("Commands:\n")
    print("  generateheader <schema.xml> <public_header.h> <service_header.h> [<profile_header.h> <profile.csv>...]")
    print("")
    print("schema.xml : An XML with the definition of a set of known")
    print("             UEFI variables ('knobs') and types to interpret them")


def main():
    if len(sys.argv) < 2:
        usage()
        sys.stderr.write('Must provide a command.\n')
        sys.exit(1)
        return

    if sys.argv[1].lower() == "generateheader":
        if len(sys.argv) < 5:
            usage()
            sys.stderr.write('Invalid number of arguments.\n')
            sys.exit(1)
            return

        schema_path = sys.argv[2]
        header_path = sys.argv[3]
        service_path = sys.argv[4]

        # Load the schema
        schema = VariableList.Schema.load(schema_path)

        generate_sources(schema, header_path, service_path)

        if len(sys.argv) >= 6:
            profile_header_path = sys.argv[5]
            profile_paths = sys.argv[6:]

            generate_profiles(schema, profile_header_path, profile_paths)
        return 0


if __name__ == '__main__':
    sys.exit(main())
