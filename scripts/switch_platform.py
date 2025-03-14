import glob, platform
import os

print("Platform switch script starting...")

is_windows = platform.system().lower() == "windows"
section = "Windows" if is_windows else "MacOS"

def process_file(file_path, lines):
    """Process file contents based on file type"""
    ext = os.path.splitext(file_path)[1].lower()
    
    # For JSON files, we need to handle the entire file differently
    if ext == '.json':
        with open(file_path, 'w') as f:
            in_platform_switch = False
            in_other_platform = False
            
            for line in lines:
                if "PLATFORM_SWITCH_START" in line:
                    in_platform_switch = True
                    f.write(line.lstrip("/"))
                elif "PLATFORM_SWITCH_END" in line:
                    in_platform_switch = False
                    f.write(line.lstrip("/"))
                elif in_platform_switch:
                    if section in line:
                        # We're in the correct platform section
                        in_other_platform = False
                        f.write(line.lstrip("/"))
                    elif "Configuration" in line:
                        # This is a platform section header
                        if section not in line:
                            in_other_platform = True
                        f.write(line.lstrip("/"))
                    else:
                        # If we're in the other platform's section, comment it
                        if in_other_platform:
                            f.write("//" + line.lstrip("/"))
                        else:
                            f.write(line.lstrip("/"))
                else:
                    # Outside platform switch block, uncomment everything
                    f.write(line.lstrip("/"))
    else:
        # For non-JSON files, use the original logic
        with open(file_path, 'w') as f:
            for line in lines:
                if "PLATFORM_SWITCH_START" in line or "PLATFORM_SWITCH_END" in line:
                    f.write(line)
                elif section in line or "PLATFORM_SWITCH" in line:
                    f.write(line.lstrip("/"))
                else:
                    f.write("//" + line.lstrip("/"))

# Process all relevant files
for file in glob.glob("**/.vscode/settings.json", recursive=True) + \
            glob.glob("**/.vscode/c_cpp_properties.json", recursive=True) + \
            glob.glob("**/.vscode/launch.json", recursive=True):
    print(f"Processing {file}")
    with open(file, 'r') as f:
        lines = f.readlines()
    process_file(file, lines)
    print(f"Updated {file}")

print("Platform switch script completed.")
