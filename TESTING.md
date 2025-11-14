# Testing Guide for Bacula Community Edition

This document provides comprehensive instructions for running the various tests available in the Bacula Community Edition repository.

## Table of Contents

- [Overview](#overview)
- [Prerequisites](#prerequisites)
- [Quick Start](#quick-start)
- [Test Types](#test-types)
- [Running Tests](#running-tests)
- [Debugging Tests](#debugging-tests)
- [Advanced Topics](#advanced-topics)

## Overview

Bacula includes an extensive regression test suite located in the `regress/` directory. The test suite includes:

- **542+ regression test scripts** covering disk, tape, and autochanger operations
- **30+ unit tests** for core data structures and utilities
- **Python tests** for Kubernetes backend plugins
- **CTest integration** for automated testing dashboards

## Prerequisites

### System Requirements

- A dedicated test system or non-production environment
- A separate database from your production Bacula installation
- Supported databases: SQLite, SQLite3, MySQL, or PostgreSQL
- For tape tests: physical tape drive or autochanger (optional)
- For plugin tests: appropriate backend services (Docker, Kubernetes, Swift, etc.)

### Important Warnings

⚠️ **CRITICAL**: Do not run tests on your production system with a production database!

- All database tables will be **deleted and recreated**
- Tape tests will **overwrite any tape** in the configured drive
- Autochanger tests will **overwrite tapes in slots 1 and 2**

### Recommended Setup

- Use a different database type than production (e.g., SQLite for testing if production uses MySQL)
- Run tests on a dedicated test machine
- Ensure SQLite is built if using it: `(cd your-depkgs; make sqlite)`

## Quick Start

### 1. Create Configuration

```bash
cd regress/
cp prototype.conf config
# Edit config file to match your system
```

The `config` file should specify:
- Database type and connection details
- Bacula source code location
- Test environment paths
- Site name for CTest dashboard (optional)

### 2. Initial Setup

```bash
make setup
```

This command (run **once**):
- Builds a Makefile from your config
- Copies and configures Bacula source
- Builds Bacula binaries
- Configures test scripts and configuration files

**Note**: Rerun `make setup` only when the source code changes.

### 3. Run Your First Test

```bash
# Run a single test
tests/backup-bacula-test

# Or run all disk-based tests
./do_disk
```

## Test Types

### Disk-Based Tests

Tests that use disk storage (safe for all environments):

- **Naming convention**: Files ending in `-test`
- **Example tests**:
  - `backup-bacula-test` - Basic backup functionality
  - `accurate-test` - Accurate backup testing
  - `two-jobs-test` - Multiple concurrent jobs
  - `migration-test` - Data migration features

### Tape-Based Tests

Tests that require physical tape drives:

- **Naming convention**: Files ending in `-tape`
- **Requirements**: One tape drive, tape must be mounted
- ⚠️ **Warning**: Will overwrite the mounted tape!

### Autochanger Tests

Tests that require tape autochangers:

- **Naming convention**: Files ending in `-changer`
- **Requirements**: Autochanger with tapes in slots 1 and 2
- ⚠️ **Warning**: Will overwrite tapes in slots 1 and 2!

### Unit Tests

Low-level tests for core components:

```bash
# Run all unit tests
./all-unittests
```

**Available unit tests** (30+ tests including):
- Data structures: `alist-unittests`, `dlist-unittests`, `htable-unittests`
- Encoding: `base32-unittests`, `base64-unittests`
- Cryptography: `crypto-unittests`, `sha1-unittests`
- Networking: `bsock-unittests`, `bsockcore-unittests`
- File operations: `breaddir-unittests`, `fnmatch-unittests`

### Plugin Tests

Tests for Bacula plugins:

- **Kubernetes backend**: See `bacula/src/plugins/fd/kubernetes-backend/tests/README`
- **Docker plugin**: Various docker-plugin tests
- **Swift/OpenStack**: Swift plugin tests
- **Other plugins**: CDP, RHV, LDAP, OpenShift plugins

## Running Tests

### All Tests Must Be Run From the `regress/` Directory

```bash
cd regress/
```

### Common Test Runners

#### Run All Disk Tests
```bash
./do_disk
```

#### Run All Tests (Disk + Tape)
```bash
./do_all
```

#### Run Individual Tests
```bash
tests/backup-bacula-test
tests/two-jobs-test
tests/migration-test
```

#### Run All Non-Root Tests
```bash
./all-non-root-tests
```

#### Run All Tests (Including Root Tests)
```bash
su
./all-tests
make reset  # Clean up root-owned files
```

### Specialized Test Categories

```bash
# Tape tests only
./do_tape
./all-tape-tests

# Autochanger tests
./all-changer-tests

# Root-required tests
./all-root-tests

# Unit tests
./all-unittests

# Development tests
./all-dev-tests

# Store manager tests
./all-store-mngr-tests
```

### CTest Integration

For automated testing with dashboard submission:

```bash
# Nightly test runs
./nightly-disk        # Disk tests only
./nightly-tape        # Tape tests only
./nightly-all         # All tests

# Experimental runs
./experimental-disk
./experimental-tape
./experimental-all
```

Results are submitted to: http://regress.bacula.org/

### Kubernetes Plugin Tests

```bash
cd bacula/src/plugins/fd/kubernetes-backend/tests/

# Set environment variables
export BE_PLUGIN_TYPE=swift
export BE_PLUGIN_VERSION=1
export BE_PLUGIN_URL=http://192.168.0.5:8080
export BE_PLUGIN_USER=test:tester
export BE_PLUGIN_PWD=testing

# Run tests
python3 -m unittest discover tests/test_baculaswift
```

See `bacula/src/plugins/fd/kubernetes-backend/tests/README` for complete details.

## Debugging Tests

### Enable Debug Output

```bash
export REGRESS_DEBUG=1
export REGRESS_WAIT=1
tests/backup-bacula-test
```

**Environment Variables**:
- `REGRESS_DEBUG=1` - Display job and debug output
- `REGRESS_WAIT=1` - Pause for manual debugger attachment

### Using GDB with Tests

**Terminal 1** (Run test):
```bash
cd regress/
export REGRESS_DEBUG=1
export REGRESS_WAIT=1
tests/backup-bacula-test
# Wait for "Start Bacula under debugger..." message
```

**Terminal 2** (Attach debugger):
```bash
cd regress/bin/
gdb bacula-dir   # or bacula-fd, bacula-sd
run -s -f
# Wait for daemon to start
```

**Terminal 1** (Continue):
```
Press Enter to continue test
# Ignore error about daemon already running
```

### Analyzing Backtraces

If a crash occurs with a backtrace:

```bash
# In GDB
info symbol 0x8082ae5
info symbol 0x8082d58
```

This converts memory addresses to function names and locations.

## Advanced Topics

### Testing Binary Installations

To test an installed Bacula (from RPM/DEB packages):

1. Edit your `config` file:
```bash
bin=/opt/bacula/bin
scripts=/opt/bacula/scripts
conf=/opt/bacula/etc
```

2. Prepare the environment:
```bash
scripts/prepare-other-loc
```

3. Run tests manually:
```bash
tests/backup-bacula-test
./all-disk-tests
```

**Limitations**:
- `./do_disk`, `./do_all`, `./nightly-*` scripts won't work
- Must use `./all-*` test scripts or individual tests
- Database backend must match installed binaries

### Making Configuration Changes

To apply configuration changes without rebuilding:

```bash
make sed
```

Use this when you've modified `.conf.in` files and don't need to rebuild Bacula.

### Adding New Tests

1. Create a shell script in `tests/` directory
2. Follow naming convention:
   - `-test` for disk tests
   - `-tape` for tape tests
   - `-changer` for autochanger tests
3. Make script executable: `chmod +x tests/your-test-name`
4. Test independently before adding to test runners

**Best practices**:
- Each test should be self-contained
- Clean up after completion
- Avoid modifying `test-bacula-dir.conf.in` if possible (create custom config instead)
- Don't rely on specific order in selection lists

### Network Configuration

If running on a disconnected system:

```bash
hostname localhost
```

Ensure hostname resolves properly or tests may fail.

### Cleanup

```bash
# Clean test artifacts
make clean

# Reset file ownership after root tests
make reset

# Complete cleanup
make distclean
```

## Test Statistics

- **Total regression tests**: 542+ executable test scripts
- **Unit tests**: 30+ binaries
- **Python tests**: 20+ test files
- **Test categories**: disk, tape, changer, plugin, unit, root, aligned
- **Configuration templates**: 72+ in `scripts/` directory
- **Support scripts**: 30+ test runners and utilities

## Documentation References

- `regress/README` - Main regression testing guide
- `regress/README.ctest` - CTest/CDash integration
- `regress/README.mysql` - MySQL-specific setup
- `regress/README.vtape` - Virtual tape configuration
- `regress/README.mingw32` - Windows testing
- `bacula/src/plugins/fd/kubernetes-backend/tests/README` - Kubernetes tests
- Main Bacula documentation: http://www.bacula.org

## Getting Help

For issues with the test suite:

1. Check test output and logs in `regress/tmp/`
2. Review the relevant README files listed above
3. Check the Bacula documentation
4. Report issues at: https://gitlab.bacula.org/bacula-community-edition/bacula-community/-/issues

## Summary

**Most common workflows**:

```bash
# First time setup
cd regress/
cp prototype.conf config
# Edit config
make setup

# Run disk tests (safe, recommended)
./do_disk

# Run all tests (if you have tape hardware)
./do_all

# Debug a failing test
export REGRESS_DEBUG=1
tests/failing-test-name

# Run unit tests
./all-unittests
```

Happy testing!
