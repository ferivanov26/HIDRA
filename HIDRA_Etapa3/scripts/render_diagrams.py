"""Renderiza os diagramas técnicos a partir das fontes Graphviz versionadas."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
for source in sorted((root / 'docs/diagramas').glob('*.dot')):
    for fmt in ('svg', 'png'):
        subprocess.run(['dot', '-T' + fmt, str(source), '-o', str(source.with_suffix('.' + fmt))], check=True)
    print(source.stem)
