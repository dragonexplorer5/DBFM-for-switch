🐉 DBFM (Dynamic Brew File Manager) for Nintendo Switch
A comprehensive all‑in‑one homebrew manager and system toolset for the Nintendo Switch, featuring dynamic applet loading and optimized resource management.

✨ Key Features
File Management
Complete file browser for SD, NAND, and USB devices

Remote PC access via USB (Quark protocol)

Advanced file operations (copy, move, delete, rename)

Hex viewer and text editor

Search functionality with filters

Bulk operations and batch processing

Size calculation and space management

Game Management
Dump and convert games (NSP/XCI/NSZ)

Install from multiple sources (SD/USB/Network)

Title management system

NCA/NSP verification

Title key and ticket management

Version control and rollback

System Tools
NAND backup and restore

emuMMC creation and management

Firmware management

Account handling

PRODINFO tools

System monitoring

Save Management
Game save backup/restore

Multi‑user save handling

Save versioning

Bulk operations

Save verification

Homebrew Management
Download and update apps

Repository system

Update checker

Dependency handling

Installation verification

Connectivity
USB connection (Quark)

Network file transfer

Remote installation

Hidden browser access

Additional Features
Dynamic applet loading

Task queue system

Custom themes

Security tools

Encrypted logging

File validation

Auto‑cleanup

🛠️ Project Structure
Code
DBFM-for-switch/
├── source/          # Source code
│   ├── core/        # Core functionality
│   ├── file/        # File management
│   ├── game/        # Game handling
│   ├── system/      # System tools
│   ├── save/        # Save management
│   ├── net/         # Network features
│   ├── ui/          # User interface
│   ├── security/    # Security features
│   ├── util/        # Utilities
│   └── applets/     # Dynamic applets
├── include/         # Header files
├── lib/             # External libraries
├── assets/          # Resources
├── tests/           # Test cases
└── docs/            # Documentation
⚡ Dynamic Loading System
DBFM optimizes Switch resources through:

Core system always loaded

Applets loaded on demand

Automatic memory cleanup

Resource monitoring

State preservation

🔧 Building
Required:

devkitPro

libnx

devkitA64

pkg‑config

make

📥 Installation
Copy dbfm.nro to /switch/ on your SD card.

Launch through hbmenu.

Optional components will be downloaded on first use.

📚 Documentation
User Guide

Developer Guide

Security Guide

API Reference

🤝 Contributing
Contributions are welcome!

Please see CONTRIBUTING.md for guidelines.

Modifications are allowed under the Dragon Protective License v3.0.

Redistributors must either:

Provide clear credit to the original authors (dragonexplorer5, XorTroll, and contributors), OR

Arrange compensation with the original authors.

🔒 Security
Encrypted logging

File validation

Security auditing

Memory protection

Access control

Operation verification

📜 License
This project is dual‑licensed under:

GPLv3 (for compatibility with Goldleaf and other GPL projects)

Dragon Protective License v3.0 (Credit/Compensation Edition)

You may redistribute and modify DBFM under either license. Redistributors must provide source code and either:

Credit the original authors, or

Arrange compensation.

See LICENSE for full terms.

🙏 Acknowledgments
Thanks to the developers of:

Goldleaf

Checkpoint

Tinfoil

Hekate

DBI
