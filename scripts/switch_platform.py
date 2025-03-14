import glob, platform

is_windows = platform.system().lower() == "windows"
section = "Windows" if is_windows else "MacOS"

for file in glob.glob("**/.vscode/settings.json", recursive=True) + glob.glob("**/.vscode/c_cpp_properties.json", recursive=True):
    with open(file, 'r') as f:
        lines = f.readlines()
    
    with open(file, 'w') as f:
        for line in lines:
            if "PLATFORM_SWITCH_START" in line or "PLATFORM_SWITCH_END" in line:
                f.write(line)
            elif section in line or "PLATFORM_SWITCH" in line:
                f.write(line.lstrip("/"))
            else:
                f.write("//" + line.lstrip("/"))
    print(f"Updated {file}")
