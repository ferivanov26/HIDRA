#!/usr/bin/env python3
"""Compõe a atualização da etapa 3 e preserva as páginas das demais etapas."""
from pathlib import Path
import argparse
import hashlib
import html
import re
import subprocess
import tempfile
import textwrap

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_JUSTIFY
from reportlab.lib.styles import ParagraphStyle
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    SimpleDocTemplate, Paragraph, Spacer, Preformatted, Image,
    Table, TableStyle, PageBreak, KeepTogether,
)

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
BASE_REVISION = 'b5fd031cefee975adf4deb2eeab9474c56af0c3a'
BASE_NAME = 'Documentos/Apostila de Projeto - Grupo 5.pdf'
BASE_SHA256 = 'd4efdf83c6a53547a57690f63c24e1700b050142d5a288e709aef28f04f86190'
PAGE_SIZE = (596, 842)
WIDTH = 468
INK = colors.HexColor('#182d34')
GRAY = colors.HexColor('#526169')
FONT_DIR = Path('/usr/share/fonts/truetype/liberation2')
if not FONT_DIR.exists():
    FONT_DIR = Path('/usr/share/fonts/truetype/liberation')
for name, filename in [
    ('Body', 'LiberationSerif-Regular.ttf'), ('Bold', 'LiberationSerif-Bold.ttf'),
    ('Italic', 'LiberationSerif-Italic.ttf'), ('Mono', 'LiberationMono-Regular.ttf'),
]:
    pdfmetrics.registerFont(TTFont(name, str(FONT_DIR / filename)))
pdfmetrics.registerFontFamily('Body', normal='Body', bold='Bold', italic='Italic', boldItalic='Bold')
STYLES = {
    'body': ParagraphStyle('body', fontName='Body', fontSize=11, leading=15.2,
                           alignment=TA_JUSTIFY, spaceAfter=9, textColor=INK),
    'h1': ParagraphStyle('h1', fontName='Bold', fontSize=16, leading=20,
                         spaceBefore=8, spaceAfter=14, keepWithNext=True, textColor=INK),
    'h2': ParagraphStyle('h2', fontName='Bold', fontSize=13, leading=17,
                         spaceBefore=15, spaceAfter=10, keepWithNext=True, textColor=INK),
    'h3': ParagraphStyle('h3', fontName='Bold', fontSize=11.5, leading=15.5,
                         spaceBefore=12, spaceAfter=8, keepWithNext=True, textColor=INK),
    'code': ParagraphStyle('code', fontName='Mono', fontSize=7.5, leading=10.2,
                           backColor=colors.HexColor('#f0f3f5'), borderPadding=7,
                           spaceBefore=6, spaceAfter=8),
    'caption': ParagraphStyle('caption', fontName='Body', fontSize=9, leading=12,
                              spaceAfter=12, alignment=TA_CENTER, textColor=GRAY),
    'cell': ParagraphStyle('cell', fontName='Body', fontSize=9.2, leading=12.2),
    'source': ParagraphStyle('source', fontName='Italic', fontSize=8.2, leading=10,
                             alignment=TA_CENTER, spaceAfter=12, textColor=GRAY),
}


def inline(text):
    escaped = html.escape(text)
    escaped = re.sub(r'`([^`]+)`', r'<font name="Mono" size="8.5">\1</font>', escaped)
    return re.sub(r'\*\*([^*]+)\*\*', r'<b>\1</b>', escaped)


def footer(offset):
    def draw(canvas, doc):
        canvas.saveState()
        canvas.setFont('Body', 8)
        canvas.setFillColor(GRAY)
        canvas.drawString(64, 813, 'HIDRA — Grupo 5 | Etapa 3')
        canvas.drawRightString(532, 813, '15/11/2026')
        canvas.drawRightString(532, 30, str(offset + doc.page))
        canvas.restoreState()
    return draw


def figure_group(figures):
    widths = [WIDTH / len(figures)] * len(figures)
    pictures, captions = [], []
    for caption, rel in figures:
        image = Image(str(HERE / rel))
        maximum_h = 430 if len(figures) == 1 else 465
        scale = min((widths[0] - 12) / image.imageWidth, maximum_h / image.imageHeight)
        image.drawWidth = image.imageWidth * scale
        image.drawHeight = image.imageHeight * scale
        pictures.append(image)
        captions.append(Paragraph(inline(caption), STYLES['caption']))
    table = Table([pictures, captions], colWidths=widths)
    table.setStyle(TableStyle([
        ('VALIGN', (0, 0), (-1, -1), 'TOP'), ('ALIGN', (0, 0), (-1, 0), 'CENTER'),
        ('LEFTPADDING', (0, 0), (-1, -1), 6), ('RIGHTPADDING', (0, 0), (-1, -1), 6),
        ('TOPPADDING', (0, 0), (-1, -1), 5), ('BOTTOMPADDING', (0, 0), (-1, -1), 4),
    ]))
    pieces = [table]
    if any('app_' in rel for _, rel in figures):
        pieces.append(Paragraph('Fonte: capturas reais fornecidas pelo responsável, 15/11/2026.', STYLES['source']))
    return KeepTogether(pieces)


def build(markdown, target, offset):
    lines = markdown.splitlines()
    flow = []
    i = 0
    while i < len(lines):
        line = lines[i].strip()
        if not line:
            i += 1
            continue
        if line == '---PAGE---':
            flow.append(PageBreak())
            i += 1
            continue
        if line.startswith('```'):
            code = []
            i += 1
            while i < len(lines) and not lines[i].startswith('```'):
                code.extend(textwrap.wrap(lines[i].expandtabs(4), width=98,
                    replace_whitespace=False, drop_whitespace=False,
                    break_long_words=True, break_on_hyphens=False) or [''])
                i += 1
            pieces = [Preformatted('\n'.join(code), STYLES['code'])]
            i += 1
            while i < len(lines) and not lines[i].strip():
                i += 1
            if i < len(lines) and lines[i].startswith('Listagem '):
                pieces.append(Paragraph(inline(lines[i]), STYLES['caption']))
                i += 1
            flow.append(KeepTogether(pieces))
            continue
        if line.startswith('#'):
            level = len(line) - len(line.lstrip('#'))
            flow.append(Paragraph(inline(line[level:].strip()), STYLES['h' + str(min(level, 3))]))
            i += 1
            continue
        if line.startswith('!['):
            figures = []
            while i < len(lines) and lines[i].startswith('![') and len(figures) < 2:
                match = re.fullmatch(r'!\[(.*)\]\((.*)\)', lines[i].strip())
                if not match:
                    raise ValueError('Figura inválida: ' + lines[i])
                figures.append(match.groups())
                i += 1
            flow.append(figure_group(figures))
            continue
        if line.startswith('|'):
            rows = []
            while i < len(lines) and lines[i].strip().startswith('|'):
                cells = [c.strip() for c in lines[i].strip().strip('|').split('|')]
                if not all(re.fullmatch(r':?-+:?', c) for c in cells):
                    rows.append([Paragraph(inline(c), STYLES['cell']) for c in cells])
                i += 1
            count = len(rows[0])
            col_widths = {2: [155, 313], 3: [118, 140, 210], 4: [100, 140, 34, 194]}.get(count, [WIDTH/count]*count)
            table = Table(rows, colWidths=col_widths, repeatRows=1, hAlign='LEFT')
            table.setStyle(TableStyle([
                ('BACKGROUND', (0, 0), (-1, 0), colors.HexColor('#e5eeee')),
                ('GRID', (0, 0), (-1, -1), .35, colors.HexColor('#a6b6bc')),
                ('VALIGN', (0, 0), (-1, -1), 'TOP'),
                ('LEFTPADDING', (0, 0), (-1, -1), 7), ('RIGHTPADDING', (0, 0), (-1, -1), 7),
                ('TOPPADDING', (0, 0), (-1, -1), 7), ('BOTTOMPADDING', (0, 0), (-1, -1), 7),
            ]))
            flow.extend([table, Spacer(1, 10)])
            continue
        paragraph = [line]
        i += 1
        while i < len(lines) and lines[i].strip() and not lines[i].startswith(('#', '```', '|', '![', '---PAGE---')):
            paragraph.append(lines[i].strip())
            i += 1
        style = STYLES['caption'] if paragraph[0].startswith('Listagem ') else STYLES['body']
        flow.append(Paragraph(inline(' '.join(paragraph)), style))
    doc = SimpleDocTemplate(str(target), pagesize=PAGE_SIZE,
        leftMargin=64, rightMargin=64, topMargin=52, bottomMargin=49,
        title='HIDRA — Etapa 3: implementação e validação', author='Grupo 5 — PCS3930')
    doc.build(flow, onFirstPage=footer(offset), onLaterPages=footer(offset))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', type=Path, help='PDF original de 65 páginas da main b5fd031; por padrão, obtido do Git.')
    args = parser.parse_args()
    markdown = (HERE / 'ETAPA_3.md').read_text()
    with tempfile.TemporaryDirectory(prefix='hidra-relatorio-') as folder:
        temp = Path(folder)
        base = temp / 'base.pdf'
        if args.base:
            base.write_bytes(args.base.read_bytes())
        else:
            base.write_bytes(subprocess.check_output(['git', 'show', f'{BASE_REVISION}:{BASE_NAME}'], cwd=ROOT))
        if hashlib.sha256(base.read_bytes()).hexdigest() != BASE_SHA256:
            raise ValueError('O conteúdo da base difere do PDF de referência da main b5fd031.')
        info = subprocess.check_output(['pdfinfo', str(base)], text=True)
        if not re.search(r'^Pages:\s+65$', info, re.MULTILINE):
            raise ValueError('A base precisa ser o relatório original de 65 páginas; não utilize o PDF já atualizado.')
        text = subprocess.check_output(['pdftotext', '-f', '65', '-l', '65', str(base), '-'], text=True)
        if '3.2.' not in text or 'Declaração de horas' not in text:
            raise ValueError('A página final da base não corresponde à estrutura esperada.')
        # A página 65 continha somente o roteiro vazio da 3.2 e a declaração de horas.
        # A declaração é preservada no fim da atualização, com os valores originais.
        addition = temp / 'atualizacao.pdf'
        build(markdown, addition, offset=64)
        standalone = markdown.split('---PAGE---\n\n# Declaração de horas')[0]
        build(standalone, HERE / 'ETAPA_3_IMPLEMENTACAO.pdf', offset=0)
        subprocess.run(['pdfseparate', '-f', '1', '-l', '64', str(base), str(temp / 'base-%03d.pdf')], check=True)
        original_pages = sorted(temp.glob('base-*.pdf'))
        combined = temp / 'relatorio-completo.pdf'
        subprocess.run(['pdfunite', *map(str, original_pages), str(addition), str(combined)], check=True)
        base_text = subprocess.check_output(['pdftotext', '-layout', '-f', '1', '-l', '64', str(base), '-'])
        output_text = subprocess.check_output(['pdftotext', '-layout', '-f', '1', '-l', '64', str(combined), '-'])
        if base_text != output_text:
            raise ValueError('As páginas preservadas sofreram alteração na extração de texto.')
        target = ROOT / BASE_NAME
        target.write_bytes(combined.read_bytes())
        print('Relatório completo:', target)
        print('Seção isolada:', HERE / 'ETAPA_3_IMPLEMENTACAO.pdf')
        print('SHA256 da base preservada:', hashlib.sha256(base.read_bytes()).hexdigest())


if __name__ == '__main__':
    main()
