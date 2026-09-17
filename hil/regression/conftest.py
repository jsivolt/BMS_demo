"""Pytest configuration for the regression framework's own host-side tests.

The scenario modules are named ``test_*.py`` because the project brief fixes those
names, but they are not pytest modules: they need a J-Link probe and CAN hardware.
Only ``tests/`` is collected, and the scenario modules are ignored explicitly so that
``python -m pytest hil`` cannot pick them up by accident.
"""

import sys
from pathlib import Path

REG_DIR = Path(__file__).resolve().parent

if str(REG_DIR) not in sys.path:
    sys.path.insert(0, str(REG_DIR))

#: Scenario modules - executed by run_regression.py, never by pytest.
collect_ignore = sorted(path.name for path in REG_DIR.glob("test_*.py"))
