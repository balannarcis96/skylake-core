#
# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Balan Narcis (balannarcis96@gmail.com)
#
import argparse
import json
import os
from typing import Dict, Any, Optional


class DefineEntry:
    """Represents a preprocessor define entry."""
    
    def __init__(self, priority: int, value: Any, desc: str, file: str, output: str):
        self.prio = priority
        self.value = value
        self.desc = desc
        self.file = file
        self.output = output  # 'public' or 'private'


class ConstexprEntry:
    """Represents a C++ constexpr entry."""
    
    def __init__(self, priority: int, value: Any, desc: str, type: str, 
                 namespace: str, file: str, output: str):
        self.prio = priority
        self.value = value
        self.desc = desc
        self.type = type
        self.file = file
        self.namespace = namespace
        self.output = output  # 'public' or 'private'


class TuningGenerator:
    """Main class for generating tuning header files from preset configurations."""
    
    def __init__(self, args):
        self.args = args
        self.defines: Dict[str, DefineEntry] = {}
        self.constexpr: Dict[str, ConstexprEntry] = {}
        self.max_prio = 0
        self.constexpr_namespace_name = ""
        self.constexpr_namespace_prio = 0
        self.files_count = 0
        self.version = 0
        self.default_output = "private"
        # Check for various ways verbose might be set
        self.verbose = args.verbose in ["True", "true", "1", "yes", "Yes", True]
    
    def _extract_value(self, entry: Any) -> Any:
        """Extract value from entry which can be string or dict."""
        return entry if isinstance(entry, str) else entry["value"]
    
    def _extract_description(self, key: str, entry: Any) -> str:
        """Extract description from entry."""
        if isinstance(entry, str):
            return key
        return key if "desc" not in entry else f"{key} - {entry['desc']}"
    
    def _extract_output_type(self, entry: Any, default: str) -> str:
        """Extract and validate output type from entry."""
        if isinstance(entry, dict) and "output" in entry:
            output = entry["output"]
            if output in ["public", "private"]:
                return output
        return default
    
    def _log(self, message: str):
        """Print message if verbose mode is enabled."""
        if self.verbose:
            print(message)
    
    def _update_constexpr_namespace(self, config: Dict, priority: int, file: str):
        """Update constexpr namespace if needed based on priority."""
        if "constexpr_namespace" not in config:
            return
        
        new_namespace = config["constexpr_namespace"]
        
        if not self.constexpr_namespace_name.strip():
            self.constexpr_namespace_name = new_namespace
            self.constexpr_namespace_prio = priority
        elif (new_namespace != self.constexpr_namespace_name and 
              self.constexpr_namespace_prio < priority):
            self._log(
                f"Constexpr namespace for target '{self.args.preset_target_name}' "
                f"was updated from '{self.constexpr_namespace_name}' priority "
                f"{self.constexpr_namespace_prio} to '{new_namespace}' priority "
                f"{priority} by preset file {file}"
            )
            self.constexpr_namespace_name = new_namespace
            self.constexpr_namespace_prio = priority
    
    def _process_defines(self, config: Dict, priority: int, file: str, 
                        current_output: str):
        """Process define entries from configuration."""
        if "defines" not in config:
            return
        
        for key, entry in config["defines"].items():
            if key in self.defines:
                self._update_existing_define(key, entry, priority, file)
            else:
                self._add_new_define(key, entry, priority, file, current_output)
    
    def _update_existing_define(self, key: str, entry: Any, priority: int, 
                                file: str):
        """Update existing define entry if priority is higher."""
        existing = self.defines[key]
        if existing.prio < priority:
            new_val = self._extract_value(entry)
            if self.verbose:
                old_val = self._extract_value(existing.value)
                self._log(
                    f"Define '{key}' was updated from '{old_val}' priority "
                    f"{existing.prio} to '{new_val}' priority {priority} by "
                    f"entry in file {file}"
                )
            existing.value = new_val
            existing.prio = priority
            existing.file = file  # Update the source file
            if isinstance(entry, dict):
                if "desc" in entry:
                    existing.desc = f"{key} - {entry['desc']}"
                if "output" in entry:
                    output = entry["output"]
                    if output in ["public", "private"]:
                        existing.output = output
        else:
            self._log(
                f"Define '{key}' NOT updated (existing priority {existing.prio} >= "
                f"new priority {priority}) in file {file}"
            )
    
    def _add_new_define(self, key: str, entry: Any, priority: int, file: str,
                       current_output: str):
        """Add new define entry."""
        value = self._extract_value(entry)
        self._log(
            f"Adding new define '{key}' with value '{value}' "
            f"priority {priority} from file {file}"
        )
        self.defines[key] = DefineEntry(
            priority,
            value,
            self._extract_description(key, entry),
            file,
            self._extract_output_type(entry, current_output)
        )
    
    def _process_constexprs(self, config: Dict, priority: int, file: str,
                           current_output: str):
        """Process constexpr entries from configuration."""
        for config_key, value in config.items():
            if config_key == "constexprs" or config_key.startswith("constexprs."):
                self._process_constexpr_group(value, priority, file, current_output)
    
    def _process_constexpr_group(self, constexprs: Dict, priority: int, 
                                 file: str, current_output: str):
        """Process a group of constexpr entries."""
        for key, entry in constexprs.items():
            if key in self.constexpr:
                self._update_existing_constexpr(key, entry, priority, file)
            else:
                self._add_new_constexpr(key, entry, priority, file, current_output)
    
    def _update_existing_constexpr(self, key: str, entry: Any, priority: int,
                                   file: str):
        """Update existing constexpr entry if priority is higher."""
        existing = self.constexpr[key]
        if existing.prio < priority:
            new_val = self._extract_value(entry)
            self._log(
                f"Constexpr '{key}' was updated from '{existing.value}' priority "
                f"{existing.prio} to '{new_val}' priority {priority} by entry "
                f"in file {file}"
            )
            existing.value = new_val
            existing.file = file  # Update the source file
            
            if isinstance(entry, dict):
                if "desc" in entry:
                    existing.desc = f"{key} - {entry['desc']}"
                if "type" in entry:
                    new_type = entry["type"]
                    self._log(
                        f"Constexpr {key} type was updated from {existing.type} "
                        f"priority {existing.prio} to {new_type} priority "
                        f"{priority} by entry in file {file}"
                    )
                    existing.type = new_type
                if "output" in entry:
                    output = entry["output"]
                    if output in ["public", "private"]:
                        existing.output = output
            
            existing.prio = priority
        else:
            self._log(
                f"Constexpr '{key}' NOT updated (existing priority {existing.prio} >= "
                f"new priority {priority}) in file {file}"
            )
    
    def _add_new_constexpr(self, key: str, entry: Any, priority: int, 
                          file: str, current_output: str):
        """Add new constexpr entry."""
        if (not isinstance(entry, dict) or 
            "type" not in entry or 
            "value" not in entry):
            print(
                f"-- ERROR: Invalid constexpr entry '{key}' in file {file}!\n"
                f"\t All new constexpr entries must be an object and have "
                f"valid 'type' and 'value' properties!"
            )
            exit(-1)
        
        self._log(
            f"Adding new constexpr '{key}' with value '{entry['value']}' "
            f"priority {priority} from file {file}"
        )
        
        self.constexpr[key] = ConstexprEntry(
            priority,
            entry["value"],
            self._extract_description(key, entry),
            entry["type"],
            entry.get("namespace", ""),
            file,
            self._extract_output_type(entry, current_output)
        )
    
    def process_preset_file(self, presets_json_file: str, target_name: str,
                           preset_name: str, preset_target_name: str):
        """Process a single preset JSON file."""
        self._log(f"-- Processing preset file {presets_json_file}")
        
        try:
            with open(presets_json_file, "r") as f:
                presets_file = json.load(f)
        except (IOError, json.JSONDecodeError) as e:
            self._log(f"-- Error reading file {presets_json_file}: {e}")
            return
        
        # Update version (fixed bug: was comparing new_version > new_version)
        new_version = int(presets_file.get("version", 0))
        if new_version > self.version:
            self.version = new_version
        
        # Get priority from the file
        file_priority = int(presets_file.get("priority", 0))
        
        # Track the maximum priority seen across all files
        if self.max_prio < file_priority:
            self._log(f"-- Updating max_prio from {self.max_prio} to {file_priority}")
            self.max_prio = file_priority
        
        # Find preset
        presets = presets_file.get("presets", [])
        found_preset = self._find_preset(presets, preset_name)
        if not found_preset:
            self._handle_preset_not_found(preset_name, presets_json_file)
            return
        
        # Find config for target
        found_config = self._find_target_config(found_preset, preset_target_name)
        if not found_config:
            self._log(
                f"-- Could not find config for target '{preset_target_name}' "
                f"under preset '{preset_name}' in file {presets_json_file}"
            )
            return
        
        self._log(f"-- Found preset '{preset_name}' with priority {file_priority} in file {presets_json_file}")
        
        # Process configuration
        self._update_constexpr_namespace(found_config, file_priority, presets_json_file)
        
        # Get output type
        current_output = found_config.get('default_output', self.default_output)
        if current_output not in ['private', 'public']:
            print(
                f"Unknown output type (expected 'private' or 'public') in the "
                f"config for target:{preset_target_name} in file {presets_json_file}"
            )
            current_output = "private"
        
        # Process defines and constexprs
        self._process_defines(found_config, file_priority, presets_json_file, current_output)
        self._process_constexprs(found_config, file_priority, presets_json_file, current_output)
    
    def _find_preset(self, presets: list, preset_name: str) -> Optional[Dict]:
        """Find preset by name in presets list."""
        for preset in presets:
            if preset.get("name") == preset_name:
                return preset
        return None
    
    def _find_target_config(self, preset: Dict, target_name: str) -> Optional[Dict]:
        """Find target configuration in preset."""
        for config in preset.get("config", []):
            if config.get("target_name") == target_name:
                return config
        return None
    
    def _handle_preset_not_found(self, preset_name: str, file: str):
        """Handle case when preset is not found."""
        self._log(f"-- Could not find preset '{preset_name}' in file {file}")
        if not self.args.default_file.strip():
            exit(0)
        # If we have a default file, just return and continue processing other files
        return
    
    def _generate_file_header(self, target_name: str, output_file: str,
                             preset_name: str) -> str:
        """Generate header comment for output file."""
        header = f"/*\n    Tuning file for {target_name}\n\n"
        header += "    ! This file is generated, do not modify !\n\n"
        header += f"    File:           {os.path.basename(output_file)}\n"
        header += f"    Preset:         {preset_name}\n"
        
        if self.args.default_file.strip():
            header += f"    Default Preset: {self.args.default_preset}\n"
        
        header += f"    Target:         {target_name}\n"
        header += f"    Version:        {self.version}\n"
        header += f"    Priority:       {self.max_prio}\n*/\n\n"
        
        return header
    
    def _write_defines_section(self, f, defines: Dict[str, DefineEntry]):
        """Write defines section to file."""
        if not defines:
            return
        
        f.write("/* DEFINES */\n")
        for key, entry in defines.items():
            f.write(f"\n/* {entry.desc} */\n")
            f.write(f"#define {key} {entry.value}\n")
    
    def _write_constexpr_section(self, f, constexpr_by_namespace: Dict):
        """Write constexpr section to file."""
        namespaces_written = 0
        
        for namespace, entries in constexpr_by_namespace.items():
            if not entries:
                continue
            
            namespaces_written += 1
            
            if namespace.strip():
                if namespaces_written > 1:
                    f.write("\n")
                f.write(f"namespace {namespace} {{\n")
            
            f.write("\n/* CONSTANTS */\n")
            
            for key, entry in entries.items():
                f.write(f"\n/* {entry.desc} */")
                f.write(f"\nconstexpr {entry.type} {key} = {entry.value};\n")
                
                # Debug logging
                if self.verbose and key == "CSerializedLoggerThreadBufferSize":
                    self._log(
                        f"-- Writing to file: {key} = {entry.value} "
                        f"(type: {entry.type}, namespace: {namespace or 'global'})"
                    )
            
            if namespace.strip():
                f.write(f"\n}} /* {namespace} */\n")
    
    def write_output_file(self, file_name: str, constexpr_by_namespace: Dict,
                         constexpr: Dict, defines: Dict, output_type: str,
                         target_name: str, preset_name: str):
        """Write output header file."""
        # Debug what we're about to write
        if self.verbose:
            self._log(f"\n-- Writing {output_type.upper()} file: {file_name}")
            if "CSerializedLoggerThreadBufferSize" in constexpr:
                entry = constexpr["CSerializedLoggerThreadBufferSize"]
                self._log(
                    f"-- About to write CSerializedLoggerThreadBufferSize: "
                    f"value='{entry.value}' to {output_type} file"
                )
            else:
                self._log(f"-- CSerializedLoggerThreadBufferSize NOT in {output_type} constexpr dict")
        
        file_header = self._generate_file_header(target_name, file_name, preset_name)
        header_guard = (
            f"SKL_{target_name.upper().replace('-', '_')}_{preset_name.upper()}_"
            f"{output_type.upper()}_H"
        )
        preset_define = f"SKL_CURRENT_PRESET_{preset_name.upper()}"
        
        with open(file_name, "w") as f:
            # Write header
            f.write(file_header)
            f.write(f"#ifndef {header_guard}\n#define {header_guard}\n\n")
            
            # Write preset define
            f.write(f"#ifndef {preset_define}\n")
            f.write(f"#define {preset_define} 1\n")
            f.write("#endif\n\n")
            
            # Include types header if needed
            if constexpr:
                f.write(f"#include {self.args.types_header}\n\n")
            
            # Write defines
            self._write_defines_section(f, defines)
            
            # Add spacing between sections
            if defines and constexpr:
                f.write("\n")
            
            # Write constexprs
            self._write_constexpr_section(f, constexpr_by_namespace)
            
            # Close header guard
            f.write(f"\n#endif // {header_guard}\n")
    
    def _organize_by_namespace(self, constexpr_dict: Dict) -> Dict:
        """Organize constexpr entries by namespace."""
        by_namespace = {}
        
        for key, entry in constexpr_dict.items():
            if not entry.namespace.strip():
                entry.namespace = self.constexpr_namespace_name
            
            if entry.namespace not in by_namespace:
                by_namespace[entry.namespace] = {}
            
            # Debug logging
            if self.verbose and key == "CSerializedLoggerThreadBufferSize":
                self._log(
                    f"-- Organizing {key}: value='{entry.value}' into namespace "
                    f"'{entry.namespace or 'global'}'"
                )
            
            by_namespace[entry.namespace][key] = entry
        
        return by_namespace
    
    def _split_by_output_type(self, entries: Dict, output_type: str) -> Dict:
        """Filter entries by output type."""
        return {k: v for k, v in entries.items() if v.output == output_type}
    
    def process_presets_files(self):
        """Main processing function for all preset files."""
        # Reset state
        self.defines.clear()
        self.constexpr.clear()
        self.constexpr_namespace_name = ""
        self.constexpr_namespace_prio = 0
        self.files_count = 0
        self.version = 0
        self.max_prio = 0
        
        # Process default file first if specified
        if self.args.default_file.strip():
            self._log(
                f"Loading default preset '{self.args.default_preset}' from "
                f"file '{self.args.default_file}'"
            )
            self.process_preset_file(
                self.args.default_file,
                self.args.target_name,
                self.args.default_preset,
                self.args.preset_target_name
            )
        
        # Process input files
        presets_files = self.args.input_file.split(";")
        self._log(f"Preset files: {presets_files}")
        
        for preset_file in presets_files:
            if not preset_file.strip():
                continue
            
            self.files_count += 1
            self.process_preset_file(
                preset_file,
                self.args.target_name,
                self.args.preset_name,
                self.args.preset_target_name
            )
        
        # Debug: Show all constexpr values before splitting
        if self.verbose and "CSerializedLoggerThreadBufferSize" in self.constexpr:
            entry = self.constexpr["CSerializedLoggerThreadBufferSize"]
            self._log(
                f"-- Before splitting - CSerializedLoggerThreadBufferSize: "
                f"value='{entry.value}' output='{entry.output}' priority={entry.prio}"
            )
        
        # Split entries by output type
        constexpr_private = self._split_by_output_type(self.constexpr, 'private')
        constexpr_public = self._split_by_output_type(self.constexpr, 'public')
        defines_private = self._split_by_output_type(self.defines, 'private')
        defines_public = self._split_by_output_type(self.defines, 'public')
        
        # Debug: Show where the value ended up
        if self.verbose:
            if "CSerializedLoggerThreadBufferSize" in constexpr_private:
                self._log("-- CSerializedLoggerThreadBufferSize is in PRIVATE output")
            if "CSerializedLoggerThreadBufferSize" in constexpr_public:
                self._log("-- CSerializedLoggerThreadBufferSize is in PUBLIC output")
        
        # Organize by namespace
        constexpr_private_by_ns = self._organize_by_namespace(constexpr_private)
        constexpr_public_by_ns = self._organize_by_namespace(constexpr_public)
        
        # Write output files
        self.write_output_file(
            self.args.output_private,
            constexpr_private_by_ns,
            constexpr_private,
            defines_private,
            "private",
            self.args.target_name,
            self.args.preset_name
        )
        
        self.write_output_file(
            self.args.output_public,
            constexpr_public_by_ns,
            constexpr_public,
            defines_public,
            "public",
            self.args.target_name,
            self.args.preset_name
        )
        
        # Print summary
        if self.verbose:
            self._log(f"\n-- Summary:")
            self._log(f"--    Defines    : {len(self.defines)}")
            self._log(f"--    Constexpr  : {len(self.constexpr)}")
            self._log(f"--    Priority   : {self.max_prio}")
            self._log(f"--    Total Files: {self.files_count}")
            
            # Show final values for debugging
            if "CSerializedLoggerThreadBufferSize" in self.constexpr:
                entry = self.constexpr["CSerializedLoggerThreadBufferSize"]
                self._log(
                    f"-- Final CSerializedLoggerThreadBufferSize: "
                    f"value='{entry.value}' priority={entry.prio} from {entry.file}"
                )


def parse_arguments():
    """Parse command line arguments."""
    parser = argparse.ArgumentParser(
        description="SkylakeLib tuning generator arguments"
    )
    
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
        "--input_file",
        required=True,
        type=str,
        help="Presets json file as input"
    )
    parser.add_argument(
        "--output_public",
        required=True,
        type=str,
        help="Output tuning header file (public)"
    )
    parser.add_argument(
        "--output_private",
        required=True,
        type=str,
        help="Output tuning header file (private)"
    )
    parser.add_argument(
        "--types_header",
        default="<skl_int>",
        type=str,
        help="Types header to include"
    )
    
    return parser.parse_args()


def main():
    """Main entry point."""
    args = parse_arguments()
    generator = TuningGenerator(args)
    generator.process_presets_files()


if __name__ == "__main__":
    main()
    