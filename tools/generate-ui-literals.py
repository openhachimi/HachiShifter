#!/usr/bin/env python3
"""Compatibility audit command: NEVER generate C++ translations again.

All language values belong in resources/lang/translations.tsv. This command
reports unmapped presentation literals as JSON only; it does not modify source
or invent translations. The old unused UiLiterals.inc remains historical data.
"""
import importlib.machinery
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
if __name__=='__main__':
    discover=importlib.machinery.SourceFileLoader('external_ui_discovery',str(ROOT/'tools/discover-ui-file-keys.py')).load_module()
    discover.main(ROOT/'docs/unmapped-ui-literals.json')
