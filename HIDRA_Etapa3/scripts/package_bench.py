#!/usr/bin/env python3
"""Empacota fontes versionadas para gravação em outro computador, sem segredos."""
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
import hashlib
import json
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / 'HIDRA_Etapa3/.runtime/releases/HIDRA_BANCADA.zip'
PREFIXES = ('HIDRA_Etapa2/', 'HIDRA_Etapa3/firmware/', 'HIDRA_Etapa3/docs/', 'Audios/')
EXTRA = {'.gitignore', 'documentacao_hardware.md', 'documentacao_orquestrador.md',
         'documentacao_iot_firebase_mobile.md', 'HIDRA_Etapa3/docs/ENTREGA_BANCADA.md'}


def main():
    tracked = subprocess.check_output(['git', 'ls-files', '-z'], cwd=ROOT).decode().split('\0')
    names = sorted({p for p in tracked if p and (p.startswith(PREFIXES) or p in EXTRA)} | EXTRA)
    manifest = {}
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    with ZipFile(OUTPUT, 'w', ZIP_DEFLATED) as archive:
        for name in names:
            if Path(name).name == 'secrets.h' or any(part in ('.runtime', '.pio', '.git') for part in Path(name).parts):
                raise RuntimeError(f'Arquivo privado ou temporário inesperado: {name}')
            data = (ROOT / name).read_bytes()
            archive.writestr('HIDRA_BANCADA/' + name, data)
            manifest[name] = hashlib.sha256(data).hexdigest()
        archive.writestr('HIDRA_BANCADA/LEIA_PRIMEIRO.md',
                         (ROOT / 'HIDRA_Etapa3/docs/ENTREGA_BANCADA.md').read_bytes())
        archive.writestr('HIDRA_BANCADA/MANIFESTO.json', json.dumps({
            'git_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT).decode().strip(),
            'files_sha256': manifest,
        }, ensure_ascii=False, indent=2) + '\n')
    with ZipFile(OUTPUT) as archive:
        assert archive.testzip() is None
        assert not any(Path(n).name == 'secrets.h' for n in archive.namelist())
    print(OUTPUT)
    print(f'{len(manifest)} arquivos de origem; {OUTPUT.stat().st_size} bytes; sem secrets.h.')


if __name__ == '__main__':
    main()
