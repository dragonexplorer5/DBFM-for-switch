# DBFM (Dynamic Brew File Manager) for Nintendo Switch

A comprehensive all-in-one homebrew manager and system toolset for the Nintendo Switch, featuring dynamic applet loading and optimized resource management.

## Key Features

### File Management
- Complete file browser for SD, NAND, and USB devices
- Remote PC access via USB (Quark protocol)
- Advanced file operations (copy, move, delete, rename)
- Hex viewer and text editor
- Search functionality with filters
- Bulk operations and batch processing
- Size calculation and space management

### Game Management
- Dump and convert games (NSP/XCI/NSZ)
- Install from multiple sources (SD/USB/Network)
- Title management system
- NCA/NSP verification
- Title key and ticket management
- Version control and rollback

### System Tools
- NAND backup and restore
- emuMMC creation and management
- Firmware management
- Account handling
- PRODINFO tools
- System monitoring

### Save Management
- Game save backup/restore
- Multi-user save handling
- Save versioning
- Bulk operations
- Save verification

### Homebrew Management
- Download and update apps
- Repository system
- Update checker
- Dependency handling
- Installation verification

### Connectivity
- USB connection (Quark)
- Network file transfer
- Remote installation
- Hidden browser access

### Additional Features
- Dynamic applet loading
- Task queue system
- Custom themes
- Security tools
- Encrypted logging
- File validation
- Auto-cleanup

## Project Structure

The project uses a modular architecture with dynamic loading:

```
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
├── lib/            # External libraries
├── assets/         # Resources
├── tests/          # Test cases
└── docs/           # Documentation
```

## Dynamic Loading System

DBFM optimizes Switch resources through:
- Core system always loaded
- Applets loaded on demand
- Automatic memory cleanup
- Resource monitoring
- State preservation

## Building

Required:
- devkitPro
- libnx
- devkitA64
- pkg-config

```bash
make
```

## Installation

1. Copy `dbfm.nro` to `/switch/` on your SD card
2. Launch through hbmenu
3. Optional components will be downloaded on first use

## Documentation

- [User Guide](docs/user_guide.md)
- [Developer Guide](docs/dev_guide.md)
- [Security Guide](docs/security.md)
- [API Reference](docs/api_ref.md)

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## Security

- Encrypted logging
- File validation
- Security auditing
- Memory protection
- Access control
- Operation verification

## License

MIT License - see [LICENSE](LICENSE)

## Acknowledgments

Thanks to the developers of:
- Goldleaf
- Checkpoint
- Tinfoil
- Hekate
- DBI

## Version History

See [CHANGELOG.md](CHANGELOG.md)