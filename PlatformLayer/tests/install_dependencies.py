#!/usr/bin/env python
"""
Install dependencies for the Windows Shared Memory Test script.
This script installs the required packages for test_win_shared_memory.py.
"""

import subprocess
import sys
import os

def install_package(package):
    """Install a package using pip."""
    print(f"Installing {package}...")
    try:
        subprocess.check_call([sys.executable, "-m", "pip", "install", package])
        print(f"Successfully installed {package}")
        return True
    except subprocess.CalledProcessError as e:
        print(f"Error installing {package}: {e}")
        return False

def main():
    """Main function."""
    print("Installing dependencies for Windows Shared Memory Test...")
    
    # List of required packages
    packages = [
        "json5"  # For comment support in JSON configuration files
    ]
    
    # Install each package
    success = True
    for package in packages:
        if not install_package(package):
            success = False
    
    if success:
        print("\nAll dependencies installed successfully!")
        print("You can now run the test script:")
        print("python test_win_shared_memory.py --create-config")
        print("python test_win_shared_memory.py --mode monitor --config shared_memory_config.json5")
    else:
        print("\nSome dependencies could not be installed.")
        print("You can still run the test script, but some features may not work.")
        print("Try installing the dependencies manually:")
        print("pip install json5")
    
    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())
