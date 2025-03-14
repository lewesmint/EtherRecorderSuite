import glob, platform, re
# This script switches platform-specific configurations in VSCode JSON files

# Determine current platform
system = platform.system().lower()
current_platform = {"darwin": "MacOS", "windows": "Windows"}.get(system, "Linux")
print(f"Platform switch script: detected {current_platform}")

def process_file(file_path):
    """Process a VSCode config file for platform switching"""
    with open(file_path) as f:
        lines = f.readlines()
    
    result = []
    in_switch_block = False
    current_section = None
    
    for line in lines:
        # Handle switch markers
        if "PLATFORM_SWITCH_START" in line:
            in_switch_block = True
            result.append(line)
            continue
        if "PLATFORM_SWITCH_END" in line:
            in_switch_block = False
            result.append(line)
            continue
        if not in_switch_block:
            result.append(line)
            continue
            
        # Inside switch block - handle section headers
        if "Configuration" in line:
            match = re.search(r'(\w+)\s+Configuration', line)
            if match:
                current_section = match[1]
            result.append(line)
            continue
            
        # Skip empty lines or unknown sections
        if not current_section or not line.strip():
            result.append(line)
            continue
        
        # Process platform-specific config lines
        is_active = current_section == current_platform
        is_commented = "//" in line
        
        if is_active and is_commented:
            # Uncomment active platform lines - only remove the //
            result.append(line.replace("//", "", 1))
        elif not is_active and not is_commented:
            # Comment inactive platform lines - preserve indentation
            indent = re.match(r'^\s*', line).group(0)
            content = line[len(indent):]
            result.append(f"{indent}//{content}")
        else:
            result.append(line)
    
    with open(file_path, 'w') as f:
        f.writelines(result)

# Find and process relevant VSCode config files
config_patterns = ["settings.json", "c_cpp_properties.json", "launch.json"]
vscode_files = [f for f in glob.glob("**/.vscode/*.json", recursive=True) 
               if any(p in f for p in config_patterns)]

for file in vscode_files:
    print(f"Processing {file}")
    process_file(file)
    print(f"Updated {file}")

print("Platform switch complete")