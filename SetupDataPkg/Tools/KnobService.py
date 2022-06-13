# @ KnobService.py
#
# Generates C header files for a firmware component to consume UEFI variables
# as configurable knobs
#
# Copyright (c) 2022, Microsoft Corporation. All rights reserved.<BR>
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
import sys
import uuid
import VariableList

def generate_public_header(schema, header_path):
    with open(header_path, 'w') as out:
        out.write("#pragma once\n")
        out.write("#include <stdint.h>\n")
        out.write("// Generated Header\n")
        out.write("//  Script: {}\n".format(sys.argv[0]))
        out.write("//  Schema: {}\n".format(schema.path))
        out.write("\n")
        out.write("// Schema-defined enums\n\n")
        for enum in schema.enums:
            if enum.help != "":
                out.write("// {}\n".format(enum.help))
            out.write("typedef enum {{\n".format(enum.name))
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
            out.write("}} {};\n".format(enum.name))
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
                        member.format.ctype,
                        member.name))
                else:
                    out.write("    {} {}[{}];\n".format(
                        member.format.ctype,
                        member.name,
                        member.count))

            out.write("}} {};\n".format(struct_definition.name))
            out.write("\n")
            pass

        out.write("// Schema-defined knobs\n\n")
        for knob in schema.knobs:
            out.write("// {} knob\n\n".format(knob.name))
            if knob.help != "":
                out.write("// {}\n".format(knob.help))

            out.write("// Get the current value of the {} knob\n".format(knob.name))
            out.write("{} config_get_{}();\n".format(
                knob.format.ctype, 
                knob.name))
            out.write("\n")
            pass

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

        out.write("typedef enum {\n")
        for knob in schema.knobs:
            out.write("    KNOB_{},\n".format(knob.name))
        out.write("    KNOB_MAX\n")
        out.write("} knob_t;\n")

        out.write("\n")
        out.write("typedef struct {\n")
        for knob in schema.knobs:
            out.write("    {} {};\n".format(
                    knob.format.ctype, 
                    knob.name))
        out.write("} knob_values_t;\n")
        out.write("\n")

        format_options = VariableList.StringFormatOptions()
        format_options.cformat = True

        out.write("knob_values_t g_knob_default_values = {\n")
        for knob in schema.knobs:
            out.write("    {},\n".format(knob.format.object_to_string(knob.default, format_options)))
        out.write("};\n\n")

        out.write("#ifdef CONFIG_INCLUDE_CACHE\n")
        out.write("knob_values_t g_knob_cached_values = {\n")
        for knob in schema.knobs:
            out.write("    {},\n".format(knob.format.object_to_string(knob.default, format_options)))
        out.write("};\n")
        out.write("#define CONFIG_CACHE_ADDRESS(knob) (&g_knob_cached_values.knob)\n")
        out.write("#else // CONFIG_INCLUDE_CACHE\n")
        out.write("#define CONFIG_CACHE_ADDRESS(knob) (NULL)\n")
        out.write("#endif // CONFIG_INCLUDE_CACHE\n")
        out.write("\n\n")

        out.write("typedef struct {\n")
        out.write("    unsigned long  Data1;\n")
        out.write("    unsigned short Data2;\n")
        out.write("    unsigned short Data3;\n")
        out.write("    unsigned char  Data4[8];\n")
        out.write("} config_guid_t;\n")
        out.write("\n")
        out.write("typedef struct {\n")
        out.write("    knob_t knob;\n")
        out.write("    void* default_value_address;\n")
        out.write("    void* cache_value_address;\n")
        out.write("    size_t value_size;\n")        
        out.write("    const char* name;\n")
        out.write("    config_guid_t vendor_namespace;\n")
        out.write("    int attributes;\n")
        out.write("} knob_data_t;\n")
        
        out.write("\n")
        out.write("knob_data_t g_knob_data[{}] = {{\n".format(len(schema.knobs) + 1))
        for knob in schema.knobs:
            out.write("    {\n")
            out.write("       KNOB_{},\n".format(knob.name))
            out.write("       &g_knob_default_values.{},\n".format(knob.name))
            out.write("       CONFIG_CACHE_ADDRESS({}),\n".format(knob.name))
            out.write("       sizeof({}),\n".format(knob.format.ctype))
            out.write("       \"{}\",\n".format(knob.name))
            out.write("       {}, // {}\n".format(format_guid(knob.namespace), knob.namespace))
            out.write("       {}\n".format(7))
            out.write("    },\n")
        out.write("    {\n");
        out.write("       KNOB_MAX,\n")
        out.write("       NULL,\n")
        out.write("       NULL,\n")
        out.write("       0,\n")
        out.write("       NULL,\n")
        out.write("       {0,0,0,{0,0,0,0,0,0,0,0}},\n")
        out.write("       0\n")
        out.write("    }\n");
        out.write("};\n\n")

        out.write("\n")
        out.write("void* get_knob_value(knob_t knob);\n")
        out.write("\n")
        out.write("// Schema-defined knobs\n\n")
        for knob in schema.knobs:
            out.write("// {} knob\n".format(knob.name))
            if knob.help != "":
                out.write("// {}\n".format(knob.help))

            out.write("// Get the current value of the {} knob\n".format(knob.name))
            out.write("{} config_get_{}() {{\n".format(
                knob.format.ctype, 
                knob.name))
            out.write("    return *(({}*)get_knob_value(KNOB_{}));\n".format(
                knob.format.ctype,
                knob.name
            ))
            out.write("}\n")
            out.write("\n")
            pass
    
