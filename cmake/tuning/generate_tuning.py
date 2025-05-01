#
# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Balan Narcis (balannarcis96@gmail.com)
#
import argparse
import json
import os

class DefineEntry:
    def __init__(self, priority, value, desc, file, output):
        self.prio = priority
        self.value = value
        self.desc = desc
        self.file = file
        self.output = output # public, private

class ConstexprEntry:
    def __init__(self, priority, value, desc, type, namespace, file, output):
        self.prio = priority
        self.value = value
        self.desc = desc
        self.type = type
        self.file = file
        self.namespace = namespace
        self.output = output # public, private

parser = argparse.ArgumentParser(description="SkylakeLib tuning generator arguments")
parser.add_argument(
    "--preset_name",
    required=True,
    type=str,
    help="Name of the preset to use for generating",
)
parser.add_argument(
    "--target_name",
    required=True,
    type=str,
    help="Name of target to generate the tuning for",
)
parser.add_argument(
    "--preset_target_name",
    required=True,
    type=str,
    help="Name of target to look for in the presets",
)
parser.add_argument(
    "--verbose",
    default=False,
    type=str,
    help="Verbose output",
)
parser.add_argument(
    "--default_file",
    default="",
    type=str,
    help="The default presets json file",
)
parser.add_argument(
    "--default_preset",
    default="default",
    type=str,
    help="The default presets name",
)
parser.add_argument(
    "--input_file", required=True, type=str, help="Presets json file as input"
)
parser.add_argument(
    "--output_public", required=True, type=str, help="Output tuning header file (public)"
)
parser.add_argument(
    "--output_private", required=True, type=str, help="Output tuning header file (private)"
)
parser.add_argument(
    "--types_header",
    default="<skl_int>",
    type=str,
    help="Types header to include"
)

args = parser.parse_args()
defines = dict()
constexpr = dict()
max_prio = 0
constexpr_namespace_name = ""
constexpr_namespace_prio = 0
files_count = 0
version = 0
default_output = "private"
verbose = args.verbose == "True"

def process_preset_file(presets_json_file, target_name, preset_name, preset_target_name):
    global defines
    global constexpr
    global max_prio
    global constexpr_namespace_name
    global constexpr_namespace_prio
    global files_count
    global version
    global default_output

    if verbose:
        print(f"-- Pricessing preset file {presets_json_file}")
    with open(presets_json_file, "r") as f:
        presets_file = json.load(f)

        new_version = int(presets_file["version"])
        if new_version > new_version:
            version = new_version

        presets = presets_file["presets"]
        if "priority" in presets_file:
            priority = int(presets_file["priority"])
        else:
            priority = 0
        if max_prio < priority:
            max_prio = priority
        found_preset = ""
        found_config = ""
        for preset in presets:
            if preset["name"] == preset_name:
                found_preset = preset
                break
        if not found_preset:
            if verbose:
                print(
                    f"-- Could not find preset '{preset_name}' in file {presets_json_file}"
                )
            if not args.default_file.strip():
                exit(0)
            else:
                return

        for config in preset["config"]:
            if config["target_name"] == preset_target_name:
                found_config = config
                break
        if not found_config:
            if verbose:
                print(
                    f"-- Could not find config for target '{preset_target_name}' under preset '{preset_name}' in file {presets_json_file}"
                )
            return
        if "constexpr_namespace" in found_config:
            if not constexpr_namespace_name.strip():
                constexpr_namespace_name = found_config["constexpr_namespace"]
                constexpr_namespace_prio = priority
            elif (found_config["constexpr_namespace"] != constexpr_namespace_name) and (
                constexpr_namespace_prio < priority
            ):
                new_name = found_config["constexpr_namespace"]
                if verbose:
                    print(
                        f"Constexpr namespace for target '{preset_target_name}' was updated from '{constexpr_namespace_name}' priority {constexpr_namespace_prio} to '{new_name}' priority {priority} by preset file {presets_json_file}"
                    )
                constexpr_namespace_name = new_name
                constexpr_namespace_prio = priority

        if 'default_output' in found_config:
            current_output = found_config['default_output']
        else:
            current_output = default_output

        if current_output != 'private' and current_output != 'public':
            print(f"Unknown output type (expected 'private' or 'public') in the config for target:{preset_target_name} in file {presets_json_file}")
            current_output = "private"

        if "defines" in found_config:
            for key, entrie in found_config["defines"].items():
                if key in defines:
                    defines_entrie = defines[key]
                    if defines_entrie.prio < priority:
                        new_val = entrie if isinstance(entrie, str) else entrie["value"]
                        if verbose:
                            tmp_val_old = (
                                defines_entrie.value
                                if isinstance(defines_entrie.value, str)
                                else defines_entrie.value["value"]
                            )
                            print(
                                f"Define '{key}' was updated from '{tmp_val_old}' priority {defines_entrie.prio} to '{new_val}' priority {priority} by entry in file {presets_json_file}"
                            )
                        # Update value
                        defines_entrie.value = new_val
                        # Update prio
                        defines_entrie.prio = priority
                        # Update desc
                        if isinstance(entrie, dict) and "desc" in entrie:
                            defines_entrie.desc = f"{key} - {entrie['desc']}"
                else:
                    # Add new defines entry
                    defines[key] = DefineEntry(
                        priority,
                        entrie if isinstance(entrie, str) else entrie["value"],
                        (
                            key
                            if isinstance(entrie, str)
                            else (
                                key
                                if not "desc" in entrie
                                else f"{key} - {entrie['desc']}"
                            )
                        ),
                        presets_json_file,
                        current_output if not "output" in entrie else current_output if (entrie["output"] != "public" and entrie["output"] != "private") else entrie["output"]
                    )
        
        for key, value in found_config.items():
            if key == "constexprs" or key.startswith("constexprs."):
                for key, entrie in value.items():
                    if key in constexpr:
                        constexpr_entrie = constexpr[key]
                        if constexpr_entrie.prio < priority:
                            new_val = entrie if isinstance(entrie, str) else entrie["value"]
                            if verbose:
                                print(
                                    f"Constexpr '{key}' was updated from '{constexpr_entrie.value}' priority {constexpr_entrie.prio} to '{new_val}' priority {priority} by entry in file {presets_json_file}"
                                )
                            # Update value
                            constexpr_entrie.value = new_val
                            # Update desc
                            if isinstance(entrie, dict) and "desc" in entrie:
                                constexpr_entrie.desc = f"{key} - {entrie['desc']}"
                            # Update type
                            if isinstance(entrie, dict) and "type" in entrie:
                                new_type = entrie["type"]
                                if verbose:
                                    print(
                                        f"Constepxr {key} type was updated from {constexpr_entrie.type} priority {constexpr_entrie.prio} to {new_type} priority {priority} by entry in file {presets_json_file}"
                                    )
                                constexpr_entrie.type = new_type
                            # Update prio
                            constexpr_entrie.prio = priority
                    else:
                        if (
                            not isinstance(entrie, dict)
                            or not "type" in entrie
                            or not "value" in entrie
                        ):
                            print(
                                f"-- ERROR: Invalid constexpr entrie '{key}' in file {presets_json_file}!\n\t All new constexpr entries must be an object and have valid 'type' and 'value' properties!"
                            )
                            exit(-1)
                        constexpr[key] = ConstexprEntry(
                            priority,
                            entrie["value"],
                            (key if not "desc" in entrie else f"{key} - {entrie['desc']}"),
                            entrie["type"],
                            "" if not "namespace" in entrie else entrie["namespace"],
                            presets_json_file,
                            current_output if not "output" in entrie else current_output if (entrie["output"] != "public" and entrie["output"] != "private") else entrie["output"]
                        )

def generate_file_header(target_name, output_file, preset_name, default_preset, version, max_prio, default_file):
    file_header = f"/*\n    Tuning file for {target_name}\n\n"
    file_header = f"{file_header}    ! This file is generated, do not modify !\n\n"
    file_header = f"{file_header}    File:           {os.path.basename(output_file)}\n"
    file_header = f"{file_header}    Preset:         {preset_name}\n"
    if default_file.strip():
        file_header = f"{file_header}    Default Preset: {default_preset}\n"
    file_header = f"{file_header}    Target:         {target_name}\n"
    file_header = f"{file_header}    Version:        {version}\n"
    file_header = f"{file_header}    Priority:       {max_prio}\n*/\n\n"
    return file_header

def write_output_file(file_name, constexpr_bt_namespace, constexpr, defines, output_type, target_name, preset_name, default_preset, version, max_prio, default_file, types_header):
    file_header = generate_file_header(target_name, file_name, preset_name, default_preset, version, max_prio, default_file)

    header_guard = (
        f"SKL_{target_name.upper().replace('-', '_')}_{preset_name.upper()}_{output_type.upper()}_H"
    )

    with open(file_name, "w") as f:
        f.write(file_header)
        f.write(f"#ifndef {header_guard}\n#define {header_guard}\n\n")

        preset_define = f"SKL_CURRENT_PRESET_{preset_name.upper()}"

        f.write(f"#ifndef {preset_define}\n")
        f.write(f"#define {preset_define} 1\n")
        f.write(f"#endif\n\n")

        if len(constexpr) > 0:
            f.write(f"#include {types_header}\n\n")

        if len(defines) > 0:
            f.write(f"/* DEFINES */\n")

        for key, entrie in defines.items():
            f.write(f"\n/* {entrie.desc} */\n")
            f.write(f"#define {key} {entrie.value}\n")

        if len(defines) > 0 and len(constexpr) > 0:
            f.write(f"\n")

        namespaces_i = 0

        for namespace, constexpr_entrie in constexpr_bt_namespace.items():
            if len(constexpr_entrie) == 0:
                continue

            namespaces_i = namespaces_i + 1

            if namespace.strip():
                if namespaces_i > 1:
                    f.write("\n")
                f.write(f"namespace {namespace}")
                f.write(" {\n")

            f.write(f"\n/* CONSTANTS */\n")

            for key, entrie in constexpr_entrie.items():
                f.write(f"\n/* {entrie.desc} */")
                f.write(f"\nconstexpr {entrie.type} {key} = {entrie.value};\n")

            if namespace.strip():
                f.write("\n}")
                f.write(f" /* {namespace} */\n")

        f.write(f"\n#endif // {header_guard}\n")

def process_presets_files(
    presets_json_file_str_list,
    target_name,
    preset_name,
    output_private_file,
    output_public_file,
    default_file,
    default_preset,
    preset_target_name,
    types_header
):
    global defines
    global constexpr
    global max_prio
    global constexpr_namespace_name
    global constexpr_namespace_prio
    global files_count
    global version

    # Reset variables
    defines.clear()
    constexpr.clear()
    constexpr_namespace_name = ""
    constexpr_namespace_prio = 0
    files_count = 0
    version = 0

    # First process the default file
    if default_file.strip():
        if verbose:
            print(
                f"Loading default preset '{default_preset}' from file '{default_file}'"
            )
        process_preset_file(default_file, target_name, default_preset, preset_target_name)

    presets_json_file_list = presets_json_file_str_list.split(";")
    if verbose:
        print(f"Preset files: {presets_json_file_list}")

    for presets_json_file in presets_json_file_list:
        if not presets_json_file.strip():
            continue

        files_count = files_count + 1
        process_preset_file(presets_json_file, target_name, preset_name, preset_target_name)

    constexpr_private = {k: v for k, v in constexpr.items() if v.output == 'private'}
    constexpr_public = {k: v for k, v in constexpr.items() if v.output == 'public'}

    defines_private = {k: v for k, v in defines.items() if v.output == 'private'}
    defines_public = {k: v for k, v in defines.items() if v.output == 'public'}

    constexpr_private_bt_namespace = dict()
    constexpr_public_bt_namespace = dict()

    for key, entrie in constexpr_private.items():
        if not entrie.namespace.strip():
            entrie.namespace = constexpr_namespace_name
        if not entrie.namespace in constexpr_private_bt_namespace:
            constexpr_private_bt_namespace[entrie.namespace] = dict()

        constexpr_private_bt_namespace[entrie.namespace][key] = entrie

    for key, entrie in constexpr_public.items():
        if not entrie.namespace.strip():
            entrie.namespace = constexpr_namespace_name
        if not entrie.namespace in constexpr_public_bt_namespace:
            constexpr_public_bt_namespace[entrie.namespace] = dict()

        constexpr_public_bt_namespace[entrie.namespace][key] = entrie

    write_output_file(
        output_private_file
        , constexpr_private_bt_namespace
        , constexpr_private
        , defines_private
        , "private"
        , target_name
        , preset_name
        , default_preset
        , version
        , max_prio
        , default_file
        , types_header
    )

    write_output_file(
        output_public_file
        , constexpr_public_bt_namespace
        , constexpr_public
        , defines_public
        , "public"
        , target_name
        , preset_name
        , default_preset
        , version
        , max_prio
        , default_file
        , types_header
    )

    if verbose:
        print(f"--    Defines    : {len(defines)}")
        print(f"--    Constexpr  : {len(constexpr)}")
        print(f"--    Priority   : {max_prio}")
        print(f"--    Total Files: {files_count}")

process_presets_files(
    args.input_file,
    args.target_name,
    args.preset_name,
    args.output_private,
    args.output_public,
    args.default_file,
    args.default_preset,
    args.preset_target_name,
    args.types_header
)
