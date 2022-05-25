## @ VariableList.py
#
# Copyright (c) 2022, Microsoft Corporation. All rights reserved.<BR>
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
##

import sys
import re
import struct
import csv
from   collections import OrderedDict
import uuid
import zlib
from xml.dom.minidom import parse, parseString

from CommonUtility import *

class ParseError(Exception):
    def __init__(self, message):
        super().__init__(message)
class InvalidNameError(ParseError):
    def __init__(self, message):
        super().__init__(message)
class InvalidTypeError(ParseError):
    def __init__(self, message):
        super().__init__(message)
class InvalidKnobError(ParseError):
    def __init__(self, message):
        super().__init__(message)

def split_braces(string_object):
    segments = []
    depth = 0
    chars = []
    complete = False
    for c in string_object.strip():
        if complete and c == ' ':
            continue
        elif complete:
            raise ParseError("Unexpected character after '}}', found '{}'".format(c))

        if c == '{':
            if depth > 0:
                chars.append(c)
            depth += 1
        elif c == ',':
            if depth > 1:
                chars.append(c)
            else:
                segments.append("".join(chars).strip())
                chars = []
        elif c == '}':
            depth -= 1
            if depth > 0:
                chars.append(c)
            elif depth == 0:
                segments.append("".join(chars).strip())
                chars = []
                complete = True
        elif depth > 0:
            chars.append(c)
        else:
            raise ParseError("expected '{{', found '{}'".format(c))
    if not complete:
        ParseError("expected '}'")

    return segments

# Checks to see if a token is a valid C identifier
def is_valid_name(token):
    matches = re.findall("^[a-zA-Z_][0-9a-zA-Z_]*$", token)
    return len(matches) == 1


class StringFormatOptions:
    def __init__(self):
        pass

    # When true, format the values for use in "C"
    # When false, format the values for use in the XML
    cformat = False


# A DataFormat defines how a variable element is serialized
# * to/from strings (as in the XML attributes) as well as
# * to/from binary (as in the variable list)
# * to C data types
class DataFormat:
    def __init__(self, ctype):
        self.ctype = ctype
        pass

# Represents all data types that have an object representation as a Python int
class IntValueFormat(DataFormat):
    def __init__(self, ctype, pack_format, csuffix = ""):
        super().__init__(ctype)
        self.pack_format = pack_format
        self.csuffix = csuffix

    def string_to_object(self, string_representation):
        try:
            return int(string_representation)
        except:
            raise ParseError("Value '{}' is not a valid number".format(string_representation))

    def object_to_string(self, object_representation, options=StringFormatOptions()):
        if options.cformat:
            return "{}{}".format(object_representation, self.csuffix)
        else:
            return str(object_representation)
    def object_to_binary(self, object_representation):
        return struct.pack(self.pack_format, object_representation)
    def binary_to_object(self, binary_representation):
        return struct.unpack(self.pack_format, binary_representation)[0]
    def size_in_bytes(self):
        return struct.calcsize(self.pack_format)

# Represents all data types that have an object representation as a Python float
class FloatValueFormat(DataFormat):
    def __init__(self, ctype, pack_format, csuffix = ""):
        super().__init__(ctype)
        self.pack_format = pack_format
        self.csuffix = csuffix

    def string_to_object(self, string_representation):
        return float(string_representation)
    def object_to_string(self, object_representation, options=StringFormatOptions()):
        return "{}{}".format(object_representation, self.csuffix)
    def object_to_binary(self, object_representation):
        return struct.pack(self.pack_format, object_representation)
    def binary_to_object(self, binary_representation):
        return struct.unpack(self.pack_format, binary_representation)[0]
    def size_in_bytes(self):
        return struct.calcsize(self.pack_format)

# Represents all data types that have an object representation as a Python bool
class BoolFormat(DataFormat):
    def __init__(self):
        super().__init__(ctype='bool')

    def string_to_object(self, string_representation):
        return string_representation.strip().lower() in ['true', '1', 'yes']
    def object_to_string(self, object_representation, options=StringFormatOptions()):
        if object_representation:
            return 'true'
        else:
            return 'false'
    def object_to_binary(self, object_representation):
        return struct.pack("<?", object_representation)
    def binary_to_object(self, binary_representation):
        return struct.unpack("<?", binary_representation)[0]
    def size_in_bytes(self):
        return struct.calcsize("<?")


builtin_types = {
    'uint8_t'  : (lambda: IntValueFormat(ctype='uint8_t', pack_format='<B')),
    'int8_t'   : (lambda: IntValueFormat(ctype='int8_t', pack_format='<b')),
    'uint16_t' : (lambda: IntValueFormat(ctype='uint16_t', pack_format='<H')),
    'int16_t'  : (lambda: IntValueFormat(ctype='int16_t', pack_format='<h')),
    'uint32_t' : (lambda: IntValueFormat(ctype='uint32_t', pack_format='<I', csuffix="ul")),
    'int32_t'  : (lambda: IntValueFormat(ctype='int32_t', pack_format='<i', csuffix="l")),
    'uint64_t' : (lambda: IntValueFormat(ctype='uint64_t', pack_format='<Q', csuffix="ull")),
    'int64_t'  : (lambda: IntValueFormat(ctype='int64_t', pack_format='<q', csuffix="ll")),
    'float'    : (lambda: FloatValueFormat(ctype='float', pack_format='<f', csuffix="f")),
    'double'   : (lambda: FloatValueFormat(ctype='double', pack_format='<d')),
    'bool'     : (lambda: BoolFormat()),
}

class EnumValue:
    def __init__(self, enum_name, xml_node):
        self.enum_name = enum_name
        self.name = xml_node.getAttribute("name")
        if not is_valid_name(self.name):
            raise InvalidNameError("Enum '{}' has invalid value name '{}'".format(enum_name, self.name))

        try:
            self.number = int(xml_node.getAttribute("value"))
        except:
            raise ParseError("Enum '{}' value '{}' does not have a valid value number".format(enum_name, self.name))

        self.help = xml_node.getAttribute("help")

    def generate_header(self, out):
        if self.help != "":
            out.write("    // {}\n".format(self.help))
        if self.number != "":
            out.write("    {}_{} = {},\n".format(self.enum_name, self.name, self.number))
        else:
            out.write("    {}_{},\n".format(self.enum_name, self.name))

class EnumFormat(DataFormat):
    def __init__(self, xml_node):
        self.name = xml_node.getAttribute("name")
        if not is_valid_name(self.name):
            raise InvalidNameError("Enum name '{}' is invalid".format(self.name))

        self.help = xml_node.getAttribute("help")
        self.values = []

        
        super().__init__(ctype=self.name)

        for value in xml_node.getElementsByTagName('Value'):
            self.values.append(EnumValue(self.name, value))
        pass

    def generate_header(self, out):
        if self.help != "":
            out.write("// {}\n".format(self.help))

        out.write("enum {} {{\n".format(self.name))
        for value in self.values:
            value.generate_header(out)
        out.write("};\n")
        out.write("\n")

    def string_to_object(self, string_representation):
        try:
            return int(string_representation)
        except:
            for value in self.values:
                if value.name == string_representation:
                    return value.number
        raise ParseError("Value '{}' is not a valid value of enum '{}'".format(string_representation, self.name))
    def object_to_string(self, object_representation, options=StringFormatOptions()):
        for value in self.values:
            if value.number == object_representation:
                if options.cformat:
                    return "{}_{}".format(self.name, value.name)
                else:
                    return value.name
        return str(object_representation)
    def object_to_binary(self, object_representation):
        return struct.pack("<i", object_representation)
    def binary_to_object(self, binary_representation):
        return struct.unpack("<i", binary_representation)[0]
    def size_in_bytes(self):
        return struct.calcsize("<i")

class StructMember:
    def __init__(self, schema, xml_node):
        self.name = xml_node.getAttribute("name")
        if not is_valid_name(self.name):
            raise InvalidNameError("Enum name '{}' is invalid".format(self.name))
        self.help = xml_node.getAttribute("help")
        count = xml_node.getAttribute("count")
        if count == "":
            self.count = 1
        else:
            self.count = int(count)

        data_type = xml_node.getAttribute("type")
        # Look up the format using the schema
        #  The format may be a built-in format (e.g. 'uint32_t') or a user defined enum or struct
        self.format = schema.get_format(data_type)

    def generate_header(self, out):
        if self.help != "":
            out.write("    // {}\n".format(self.help))
        if self.count == "":
            out.write("    {} {};\n".format(self.format.ctype, self.name))
        else:
            out.write("    {} {}[{}];\n".format(self.format.ctype, self.name, self.count))

    def string_to_object(self, string_representation):
        if self.count != 1:
            segments = split_braces(string_representation)
            object_array = []
            for element in segments:
                object_array.append(self.format.string_to_object(element))
            return object_array
        else:
            return self.format.string_to_object(string_representation)
    def object_to_string(self, object_representation, options=StringFormatOptions()):
        if self.count == 1:
            return self.format.object_to_string(object_representation, options)
        else:
            element_strings = []
            for element in object_representation:
                element_strings.append(self.format.object_to_string(element, options))
            return "{{{}}}".format(",".join(element_strings))

    
    def object_to_binary(self, object_representation):
        if self.count == 1:
            return self.format.object_to_binary(object_representation)
        else:
            binary = b''
            for element in object_representation:
                binary += self.format.object_to_binary(element)
            return binary

    def binary_to_object(self, binary_representation):
        if self.count == 1:
            return self.format.binary_to_object(binary_representation)
        else:
            values = []
            element_size = self.format.size_in_bytes()
            for i in range(self.count):
                element_binary = binary_representation[(i * element_size):((i+1) * element_size)]
                values.append(self.format.binary_to_object(element_binary))
            return values

    def size_in_bytes(self):
        return self.format.size_in_bytes() * self.count

class StructFormat(DataFormat):
    def __init__(self, schema, xml_node):
        self.name = xml_node.getAttribute("name")
        if not is_valid_name(self.name):
            raise InvalidNameError("Struct name '{}' is invalid".format(self.name))
        self.help = xml_node.getAttribute("help")
        self.members = []

        super().__init__(ctype=self.name)

        for member in xml_node.getElementsByTagName('Member'):
            self.members.append(StructMember(schema, member))

        pass

    def generate_header(self, out):
        if self.help != "":
            out.write("// {}\n".format(self.help))

        out.write("typedef struct {{\n".format(self.name))
        for member in self.members:
            member.generate_header(out)
        out.write("}} {};\n".format(self.name))
        out.write("\n")

    def string_to_object(self, string_representation):
        obj = OrderedDict()

        segments = split_braces(string_representation)
            
        for (member, value) in zip(self.members, segments):
            obj[member.name] = member.string_to_object(value)
        return obj
        
    def object_to_string(self, object_representation, options=StringFormatOptions()):
        member_strings = []

        for member in self.members:
            value = object_representation[member.name]
            member_strings.append(member.object_to_string(value, options))

        return "{{{}}}".format(",".join(member_strings))
    def object_to_binary(self, object_representation):
        binary_representation = b''
        for member in self.members:
            value = object_representation[member.name]
            member_binary = member.object_to_binary(value)
            binary_representation += member_binary
            assert len(member_binary) == member.size_in_bytes()
        return binary_representation
    def binary_to_object(self, binary_representation):
        position = 0
        obj = OrderedDict()
        for member in self.members:
            member_binary = binary_representation[position:(position+member.size_in_bytes())]
            member_value = member.binary_to_object(member_binary)
            obj[member.name] = member_value
            position += member.size_in_bytes()
        return obj

    def size_in_bytes(self):
        total_size = 0
        for member in self.members:
            total_size += member.size_in_bytes()
        return total_size

class Knob:
    def __init__(self, schema, xml_node, namespace):
        self.name = xml_node.getAttribute("name")
        if not is_valid_name(self.name):
            raise InvalidNameError("Knob name '{}' is invalid".format(self.name))
        self.help = xml_node.getAttribute("help")
        self.namespace = namespace

        data_type = xml_node.getAttribute("type")
        # Look up the format using the schema
        #  The format may be a built-in format (e.g. 'uint32_t') or a user defined enum or struct
        self.format = schema.get_format(data_type)

        # Use the format to decode the default value from its string representation
        try:
            self.default = self.format.string_to_object(xml_node.getAttribute("default"))
        except ParseError as e:
            # Capture the ParseError and create a more specific error message
            raise ParseError("Unable to parse default value of '{}': {}".format(self.name, e))

        print("Knob '{}', default='{}'".format(self.name, self.format.object_to_string(self.default)))
        pass

    def generate_header(self, out):
        out.write("// {} knob\n".format(self.name))
        if self.help != "":
            out.write("// {}\n".format(self.help))

        format_options = StringFormatOptions()
        format_options.cformat = True

        out.write("// Namspace GUID for {}\n".format(self.name))
        out.write("#define CONFIG_NAMESPACE_{} \"{}\"\n\n".format(self.name, self.namespace))

        out.write("// Get the default value of {}\n".format(self.name))
        out.write("inline {} config_get_default_{}() {{\n".format(self.format.ctype, self.name));
        out.write("    return {};\n".format(self.format.object_to_string(self.default, format_options)))
        out.write("}\n")
        out.write("// Get the current value of {}\n".format(self.name))
        out.write("inline {} config_get_{}() {{\n".format(self.format.ctype, self.name));
        out.write("    {} result = config_get_default_{}();\n".format(self.format.ctype, self.name));
        out.write("    size_t value_size = sizeof(result);\n");
        out.write("    config_result_t read_result = _config_get_knob_value_by_name(\"{}\", CONFIG_NAMESPACE_{}, &value_size, (unsigned char*)&result);\n".format(self.name, self.name))
        out.write("    // i.e. an enum may be stored as a single byte, or four bytes; it may use however many bytes are needed\n")
        out.write("    if (read_result == CONFIG_RESULT_SUCCESS && value_size == sizeof(read_result)) {\n")
        out.write("        return result;\n")
        out.write("    } else {\n")
        out.write("        return config_get_default_{}();\n".format(self.name))
        out.write("    }\n")
        out.write("}\n")
        out.write("#ifdef CONFIG_SET_VALUES\n")
        out.write("// Set the current value of {}\n".format(self.name))
        out.write("inline config_result_t config_set_{}({} value) {{\n".format(self.name, self.format.ctype));
        out.write("    return _config_set_knob_value_by_name(\"{}\", CONFIG_NAMESPACE_{}, sizeof(value), (unsigned char*)&value);\n".format(self.name, self.name))
        out.write("}\n")
        out.write("#endif\n")
        out.write("\n\n");

class Schema:
    def __init__(self, dom):
        self.enums = []
        self.structs = []
        self.knobs = []

        for section in dom.getElementsByTagName('Enums'):
            for enum in section.getElementsByTagName('Enum'):
                self.enums.append(EnumFormat(enum))

        for section in dom.getElementsByTagName('Structs'):
            for struct in section.getElementsByTagName('Struct'):
                self.structs.append(StructFormat(self, struct))

        for section in dom.getElementsByTagName('Knobs'):
            namespace = section.getAttribute('namespace')
            for knob in section.getElementsByTagName('Knob'):
                self.knobs.append(Knob(self, knob, namespace))
        pass

    def parse(path):
        return Schema(parse(path))
    def parseString(string):
        return Schema(parseString(string))

    def get_knob(self, knob_name):
        for knob in self.knobs:
            if knob.name == knob_name:
                return knob
        
        raise InvalidKnobError("Knob '{}' is not defined".format(knob_name))

    def get_format(self, type_name):

        type_name = type_name.strip()

        # Look for the type in the set of built in types
        if type_name in builtin_types:
            # Construct a new instance of the built in type format
            return builtin_types[type_name]()

        # Check for custom enum or struct types

        for enum in self.enums:
            if enum.name == type_name:
                return enum

        for struct in self.structs:
            if struct.name == type_name:
                return struct
        

        raise InvalidTypeError("Data type '{}' is not defined".format(type_name))


# Generates a C header with accessors for the knobs
def generate_header(schema_path, header_path):
    
    schema = Schema.parse(schema_path)
    
    with open(header_path, 'w') as out:
        out.write("#pragma once\n")
        out.write("#include <stdint.h>\n")
        out.write("// Generated Header\n")
        out.write("// Script: {}\n".format(sys.argv[0]))
        out.write("// Schema: {}\n".format(schema_path))
        out.write("\n")
        out.write("// config_common.h includes helper functions for the generated interfaces that\n")
        out.write("// do not require code generation themselves\n")
        out.write("#include \"config_common.h\"\n")
        out.write("\n")
        for enum in schema.enums:
            enum.generate_header(out)
            pass
        for struct in schema.structs:
            struct.generate_header(out)
            pass
        for knob in schema.knobs:
            knob.generate_header(out)
            pass

    
    pass


class UEFIVariable:
    def __init__(self, name, guid, data, attributes = 7):
        self.name = name
        if isinstance(guid,uuid.UUID):
            self.guid = guid
        else:
            self.guid = uuid.UUID(guid)
        self.attributes = attributes
        self.data = data

# Writes a vlist entry to the vlist file
def write_vlist_entry(file, variable):

    # Each entry has the following values
    #  NameSize(int32, size of Name in bytes), 
    #  DataSize(int32, size of Data in bytes), 
    #  Name(null terminated UTF-16LE encoded name), 
    #  Guid(16 Byte namespace Guid), 
    #  Attributes(int32 UEFI attributes), 
    #  Data(bytes), 
    #  CRC32(int32 checksum of all bytyes from NameSize through Datta)

    # Null terminated UTF-16LE encoded name
    name = (variable.name + "\0").encode("utf-16le")
    data = variable.data
    guid = variable.guid.bytes
    attributes = variable.attributes

    payload = struct.pack("<ii", len(name), len(data)) + name + guid + struct.pack("<I", attributes) + data
    crc = zlib.crc32(payload)

    file.write(payload)
    file.write(struct.pack("<I", crc))
    pass

def read_vlist(file):
    variables = []
    while True:
        name_size_bytes = file.read(4)
        if len(name_size_bytes) == 0:
            return variables
        
        name_size = struct.unpack("<i", name_size_bytes)[0]
        data_size = struct.unpack("<i", file.read(4))[0]

        guid_size = 16
        attr_size = 4

        remaining_bytes = name_size + guid_size + attr_size + data_size

        payload = struct.pack("<ii", name_size, data_size) + file.read(remaining_bytes)

        crc = struct.unpack("<I", file.read(4))[0]

        if crc != zlib.crc32(payload):
            raise Exception("CRC mismatch")
        
        name = payload[8:(name_size+8)].decode(encoding="UTF-16LE").strip("\0")
        guid_bytes = payload[(name_size+8):(name_size+8+16)]
        guid = uuid.UUID(bytes=guid_bytes)
        attributes_bytes = payload[(name_size+8+16):(name_size+8+16+4)]
        attributes = struct.unpack("<I", attributes_bytes)[0]
        data = payload[(name_size+8+16+4):]

        variables.append(UEFIVariable(name, guid, data, attributes))
        

# Generates a binary vlist file with the content of a set of knobs
def write_vlist(schema_path, values_path, vlist_path):

    schema = Schema.parse(schema_path)

    with open(values_path, 'r') as csv_file:
        with open(vlist_path, 'wb') as vlist_file:
            reader = csv.reader(csv_file)
            header = next(reader)

            if len(header) < 2 or header[0] != "Knob" or header[1] != "Value":
                raise ParseError("CSV is missing 'Knob' and 'Value' column headers")

            for row in reader:
                knob_name = row[0]
                knob_value_string = row[1]

                knob = schema.get_knob(knob_name)
                value = knob.format.string_to_object(knob_value_string)
                value_bytes = knob.format.object_to_binary(value)

                variable = UEFIVariable(knob.name, knob.namespace, value_bytes)
                write_vlist_entry(vlist_file, variable)
    pass

# Generates a binary vlist file with the defaults from a schema
def write_default_vlist(schema_path, vlist_path):

    schema = Schema.parse(schema_path)

    with open(vlist_path, 'wb') as vlist_file:
    
        for knob in schema.knobs:
            value_bytes = knob.format.object_to_binary(knob.default)

            variable = UEFIVariable(knob.name, knob.namespace, value_bytes)
            write_vlist_entry(vlist_file, variable)
    pass


def write_default_csv(schema_path, csv_path):

    schema = Schema.parse(schema_path)

    with open(csv_path, 'w', newline='') as csv_file:
        writer = csv.writer(csv_file)
        writer.writerow(['Knob', 'Value'])

        for knob in schema.knobs:
            writer.writerow([knob.name, knob.format.object_to_string(knob.default)])
            
    pass


def write_csv(schema_path, vlist_path, csv_path):

    schema = Schema.parse(schema_path)

    with open(vlist_path, 'rb') as vlist_file:
        with open(csv_path, 'w', newline='') as csv_file:
            writer = csv.writer(csv_file)
            writer.writerow(['Knob', 'Value'])

            for variable in read_vlist(vlist_file):

                knob = schema.get_knob(variable.name)
                value = knob.format.binary_to_object(variable.data)

                writer.writerow([knob.name, knob.format.object_to_string(value)])

    pass

def usage():
    print("Commands:\n")
    print("  generateheader <schema.xml> <header.h>")
    print("  writevl <schema.xml> [<values.csv>] <blob.vl>")
    print("  writecsv <schema.xml> [<blob.vl>] <values.csv>")

def main():
    if len(sys.argv) < 2:
        usage()
        sys.stderr.write('Must provide a command.\n')
        sys.exit(1)
        return

    if sys.argv[1].lower() == "generateheader":
        if len(sys.argv) != 4:
            usage()
            sys.stderr.write('Invalid number of arguments.\n')
            sys.exit(1)
            return

        schema_path = sys.argv[2]
        header_path = sys.argv[3]

        generate_header(schema_path, header_path)
    if sys.argv[1].lower() == "writevl":
        if len(sys.argv) == 4:
            schema_path = sys.argv[2]
            vlist_path = sys.argv[3]
            write_default_vlist(schema_path, vlist_path)
        elif len(sys.argv) == 5:
            schema_path = sys.argv[2]
            values_path = sys.argv[3]
            vlist_path = sys.argv[4]
            write_vlist(schema_path, values_path, vlist_path)
        else:
            usage()
            sys.stderr.write('Invalid number of arguments.\n')
            sys.exit(1)
            return

    if sys.argv[1].lower() == "writecsv":
        if len(sys.argv) == 4:
            schema_path = sys.argv[2]
            csv_path = sys.argv[3]
            write_default_csv(schema_path, csv_path)
        elif len(sys.argv) == 5:
            schema_path = sys.argv[2]
            vlist_path = sys.argv[3]
            csv_path = sys.argv[4]
            write_csv(schema_path, vlist_path, csv_path)
        else:
            usage()
            sys.stderr.write('Invalid number of arguments.\n')
            sys.exit(1)
            return
            

if __name__ == '__main__':
    sys.exit(main())



#  schema.xml values.xml -> blob.vl
#  schema.xml blob.vl -> values.xml
#  schema.xml -> config.h
#  blob.vl -> partition.bin