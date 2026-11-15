#!/usr/bin/env python3
"""Preenche a etapa 3 reutilizando o OOXML do DOCX de referência."""
from copy import deepcopy
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
import argparse
import hashlib
import re
import textwrap

from lxml import etree as E
from PIL import Image

HERE = Path(__file__).resolve().parent
NS = {
    'w': 'http://schemas.openxmlformats.org/wordprocessingml/2006/main',
    'r': 'http://schemas.openxmlformats.org/officeDocument/2006/relationships',
    'wp': 'http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing',
    'a': 'http://schemas.openxmlformats.org/drawingml/2006/main',
    'pic': 'http://schemas.openxmlformats.org/drawingml/2006/picture',
}
REL = 'http://schemas.openxmlformats.org/package/2006/relationships'
CT = 'http://schemas.openxmlformats.org/package/2006/content-types'


def q(name):
    prefix, local = name.split(':')
    return '{' + NS[prefix] + '}' + local


def node(name, attrs=None, text=None):
    el = E.Element(q(name))
    for key, value in (attrs or {}).items():
        el.set(q(key) if ':' in key else key, str(value))
    if text is not None:
        el.text = text
    return el


def text(el):
    return ''.join(el.xpath('.//w:t/text()', namespaces=NS))


def child(parent, name):
    found = parent.find(q(name))
    if found is None:
        found = node(name)
        parent.append(found)
    return found


def value(parent, name, val):
    child(parent, name).set(q('w:val'), str(val))


def serialized(el):
    return E.tostring(el, xml_declaration=True, encoding='UTF-8', standalone=True)


def format_code(lines, language):
    source = '\n'.join(lines)
    if language == 'cpp' and source.startswith('bool iot_publish('):
        return """bool iot_publish(const HidraEvent& event) {
    if(!ready || !valid(event)) {
        error(-1);
        return false;
    }
    Queued q{};
    q.event=event;
    q.uptime=monotonicMs();
    q.occurred=utcValid()
        ? static_cast<int64_t>(time(nullptr))*1000
        : -1;
    portENTER_CRITICAL(&mux);
    q.counter=++counter;
    portEXIT_CRITICAL(&mux);
    if(xQueueSend(queue,&q,0)!=pdTRUE) {
        loss();
        return false;
    }
    return true;
}""".splitlines()
    if language == 'cpp' and source.startswith('void iot_begin('):
        return """void iot_begin(const IotConfig& config) {
    HidraIoT::IotConfig transport{
        config.device_id, config.wifi_ssid,
        config.wifi_password, config.database_url,
        config.api_key, config.device_email,
        config.device_password, HIDRA_GOOGLE_ROOTS,
        hidra_boot_id()
    };
    HidraIoT::iot_begin(transport);
}""".splitlines()
    if language == 'kotlin' and source.startswith('fun ActivityEvent.timeLabel'):
        return """fun ActivityEvent.timeLabel(): String = when {
    occurredAtMs != null -> formatDate(occurredAtMs)
    receivedAtMs != null ->
        "Recebido em ${formatDate(receivedAtMs)}" +
        " · horário do evento não sincronizado"
    else ->
        "Horário do evento não registrado; " +
        "a ordem foi preservada."
}""".splitlines()
    if language == 'kotlin' and source.startswith('val stale='):
        return ['val stale=status.lastSeenMs==null ||',
                '    s.now-status.lastSeenMs>90000']
    if language == 'text' and source.startswith('caregiverDevices/'):
        return """caregiverDevices/{caregiver_uid}/{device_id} = true
deviceOwners/{device_id} = device_uid
devices/{device_id}/
    metadata
    status
    sessions/{session_id}/
        summary
        events/{event_id}/
            payload
            received_at_ms""".splitlines()
    result = []
    for line in lines:
        if language == 'ini' and line.startswith('build_flags = '):
            result.append('build_flags =')
            result.extend('    ' + flag for flag in line.split(' = ', 1)[1].split())
        elif language == 'bash' and len(line)>68 and not line.startswith('#'):
            pieces = textwrap.wrap(line, width=65, subsequent_indent='    ',
                break_long_words=False, break_on_hyphens=False)
            result.extend(p + (' \\' if i<len(pieces)-1 else '') for i,p in enumerate(pieces))
        elif language == 'cpp' and 'JsonVariantConst>() != event.' in line:
            left, right = line.split(' != ', 1)
            result.extend([left + ' !=', '           ' + right])
        elif language == 'cpp' and '; ' in line and len(line)>68:
            prefix = line[:len(line)-len(line.lstrip())]
            pieces = line.strip().split('; ')
            result.extend(prefix + part + (';' if i<len(pieces)-1 else '') for i,part in enumerate(pieces))
        elif language == 'text' and line.startswith('EVT ') and '{' in line:
            prefix, payload = line.split('{', 1)
            result.extend([prefix, '    {'+payload])
        elif language == 'text' and line.startswith('IoT:') and len(line)>68:
            left,right=line.split(' pendentes=',1)
            result.extend([left,'    pendentes='+right])
        else:
            result.append(line)
    return result


class Builder:
    def __init__(self, template):
        with ZipFile(template) as package:
            self.parts = {name: package.read(name) for name in package.namelist()}
        self.doc = E.fromstring(self.parts['word/document.xml'])
        self.styles = E.fromstring(self.parts['word/styles.xml'])
        self.numbers = E.fromstring(self.parts['word/numbering.xml'])
        self.rels = E.fromstring(self.parts['word/_rels/document.xml.rels'])
        self.types = E.fromstring(self.parts['[Content_Types].xml'])
        self.body = self.doc.find('w:body', NS)
        self.sect = deepcopy(self.body.find('w:sectPr', NS))
        self.original = list(self.body)
        paragraphs = self.body.findall('w:p', NS)
        self.heading = {
            1: next(p for p in paragraphs if text(p).startswith('Etapa 3 - Comunicação')),
            2: next(p for p in paragraphs if text(p) == 'Implementação da etapa 3'),
            3: next(p for p in paragraphs if text(p) == 'Organização do módulo IoT no ESP32'),
            4: next(p for p in paragraphs if text(p) == 'Interface com orquestrador'),
        }
        self.first = next(p for p in paragraphs if text(p).startswith('A etapa 3 compreende'))
        self.normal = next(p for p in paragraphs if text(p).startswith('O módulo IoT será organizado'))
        self.caption = next(p for p in paragraphs if text(p).startswith('Arquitetura da etapa 3 e integração'))
        tables = self.body.findall('w:tbl', NS)
        self.table_model = next(t for t in tables if text(t).startswith('ComponenteFunçãoAuthRepository'))
        self.code_model = next(t for t in tables if text(t).startswith('void iot_begin('))
        self.draw_id = max(map(int, self.doc.xpath('//wp:docPr/@id', namespaces=NS)), default=0) + 1
        self.rel_id = max([int(x.get('Id')[3:]) for x in self.rels if x.get('Id', '').startswith('rId')], default=0) + 1
        self.added_images = 0
        self.start_paragraph = True
        # Materializar a fonte pretendida evita herdar Calibri/Arial do aplicativo de destino.
        defaults = child(child(self.styles, 'w:docDefaults'), 'w:rPrDefault')
        default_rpr = child(defaults, 'w:rPr')
        fonts = child(default_rpr, 'w:rFonts')
        for key in ['ascii', 'hAnsi', 'eastAsia', 'cs']:
            fonts.set(q('w:' + key), 'Times New Roman')
        # O documento isolado começa na etapa 3, e não reinicia a numeração em 1.
        num = self.numbers.find('w:num[@w:numId="1"]', NS)
        override = node('w:lvlOverride', {'w:ilvl': 0})
        override.append(node('w:startOverride', {'w:val': 3}))
        num.append(override)

    def effective_run(self, paragraph):
        props = {}
        def merge(rpr):
            if rpr is not None:
                for item in rpr:
                    props[item.tag] = deepcopy(item)
        merge(self.styles.find('w:docDefaults/w:rPrDefault/w:rPr', NS))
        def style_props(sid):
            style = self.styles.find(f'w:style[@w:styleId="{sid}"]', NS)
            if style is None:
                return
            based = style.find('w:basedOn', NS)
            if based is not None:
                style_props(based.get(q('w:val')))
            merge(style.find('w:rPr', NS))
        sid = paragraph.find('w:pPr/w:pStyle', NS)
        style_props(sid.get(q('w:val')) if sid is not None else 'Normal')
        merge(paragraph.find('w:pPr/w:rPr', NS))
        rpr = node('w:rPr')
        for item in props.values():
            rpr.append(item)
        return rpr

    def paragraph(self, content, model=None):
        model = model if model is not None else self.normal
        paragraph = node('w:p')
        ppr = model.find('w:pPr', NS)
        if ppr is not None:
            paragraph.append(deepcopy(ppr))
        base = self.effective_run(model)
        for fragment in re.split(r'(\*\*.*?\*\*)', content):
            if not fragment:
                continue
            rpr = deepcopy(base)
            if fragment.startswith('**'):
                fragment = fragment[2:-2]
                value(rpr, 'w:b', 1)
            fragment = fragment.replace('`', '')
            run = node('w:r')
            run.append(rpr)
            t = node('w:t', text=fragment)
            t.set('{http://www.w3.org/XML/1998/namespace}space', 'preserve')
            run.append(t)
            paragraph.append(run)
        return paragraph

    def heading_paragraph(self, title):
        match = re.match(r'^(3(?:\.\d+)*)(?:\.)?\s+(.+)$', title)
        if not match:
            raise ValueError('Título sem hierarquia: ' + title)
        level = len(match[1].split('.'))
        self.body.append(self.paragraph(match[2], self.heading[level]))
        self.start_paragraph = True

    def add_body(self, content):
        self.body.append(self.paragraph(content, self.first if self.start_paragraph else self.normal))
        self.start_paragraph = False

    def add_caption(self, content):
        content = re.sub(r'^(?:Listagem|Figura)\s+3\.2-[\da-z]+\s+—\s*', '', content)
        self.body.append(self.paragraph(content, self.caption))

    def add_table(self, rows):
        table = node('w:tbl')
        table.append(deepcopy(self.table_model.find('w:tblPr', NS)))
        cols = len(rows[0])
        widths = [9029 // cols] * cols
        widths[-1] += 9029 - sum(widths)
        grid = node('w:tblGrid')
        for width in widths:
            grid.append(node('w:gridCol', {'w:w': width}))
        table.append(grid)
        models = self.table_model.findall('w:tr', NS)
        for index, cells in enumerate(rows):
            source_row = models[0 if index == 0 else 1]
            source_cell = source_row.find('w:tc', NS)
            row = node('w:tr')
            rpr = deepcopy(source_row.find('w:trPr', NS))
            if rpr is None:
                rpr = node('w:trPr')
            if index == 0:
                value(rpr, 'w:tblHeader', 1)
            value(rpr, 'w:cantSplit', 1)
            row.append(rpr)
            for col, content in enumerate(cells):
                cell = node('w:tc')
                props = deepcopy(source_cell.find('w:tcPr', NS))
                child(props, 'w:tcW').set(q('w:w'), str(widths[col]))
                child(props, 'w:tcW').set(q('w:type'), 'dxa')
                cell.append(props)
                cell.append(self.paragraph(content, source_cell.find('w:p', NS)))
                row.append(cell)
            table.append(row)
        self.body.append(table)
        self.body.append(self.paragraph(''))

    def add_code(self, lines, language):
        # Reservar uma página para este bloco impede separar a legenda na exportação.
        if language == 'kotlin' and lines and lines[0].startswith('fun ActivityEvent.timeLabel'):
            boundary = self.paragraph('')
            ppr = child(boundary, 'w:pPr')
            value(ppr, 'w:pageBreakBefore', 1)
            spacing = child(ppr, 'w:spacing')
            spacing.set(q('w:before'), '0')
            spacing.set(q('w:after'), '0')
            spacing.set(q('w:line'), '20')
            spacing.set(q('w:lineRule'), 'exact')
            self.body.append(boundary)
        lines = format_code(lines, language)
        table = node('w:tbl')
        table.append(deepcopy(self.code_model.find('w:tblPr', NS)))
        grid = node('w:tblGrid')
        grid.append(node('w:gridCol', {'w:w': 9029}))
        table.append(grid)
        row = node('w:tr')
        rowpr = node('w:trPr')
        value(rowpr, 'w:cantSplit', 1)
        row.append(rowpr)
        source_cell = self.code_model.find('w:tr/w:tc', NS)
        cell = node('w:tc')
        cell.append(deepcopy(source_cell.find('w:tcPr', NS)))
        for line in lines:
            paragraph = self.paragraph(line, source_cell.find('w:p', NS))
            value(child(paragraph, 'w:pPr'), 'w:keepNext', 1)
            cell.append(paragraph)
        row.append(cell)
        table.append(row)
        self.body.append(table)

    def image_run(self, path, width_pt, height_pt, description):
        rel_id = 'rId' + str(self.rel_id)
        self.rel_id += 1
        filename = 'hidra_etapa3_' + str(self.added_images) + path.suffix.lower()
        self.added_images += 1
        self.parts['word/media/' + filename] = path.read_bytes()
        relationship = E.SubElement(self.rels, '{' + REL + '}Relationship')
        relationship.set('Id', rel_id)
        relationship.set('Type', NS['r'] + '/image')
        relationship.set('Target', 'media/' + filename)
        ext = path.suffix[1:].lower()
        if not any(x.get('Extension') == ext for x in self.types):
            default = E.SubElement(self.types, '{' + CT + '}Default')
            default.set('Extension', ext)
            default.set('ContentType', 'image/jpeg' if ext in ('jpg', 'jpeg') else 'image/png')
        cx, cy = round(width_pt * 12700), round(height_pt * 12700)
        run = node('w:r')
        drawing = node('w:drawing')
        inline = node('wp:inline', {'distT': 0, 'distB': 0, 'distL': 0, 'distR': 0})
        inline.append(node('wp:extent', {'cx': cx, 'cy': cy}))
        inline.append(node('wp:docPr', {'id': self.draw_id, 'name': filename, 'descr': description}))
        self.draw_id += 1
        graphic = node('a:graphic')
        data = node('a:graphicData', {'uri': NS['pic']})
        picture = node('pic:pic')
        nv = node('pic:nvPicPr')
        nv.append(node('pic:cNvPr', {'id': 0, 'name': filename}))
        nv.append(node('pic:cNvPicPr'))
        picture.append(nv)
        fill = node('pic:blipFill')
        fill.append(node('a:blip', {'r:embed': rel_id}))
        stretch = node('a:stretch')
        stretch.append(node('a:fillRect'))
        fill.append(stretch)
        picture.append(fill)
        shape = node('pic:spPr')
        transform = node('a:xfrm')
        transform.append(node('a:off', {'x': 0, 'y': 0}))
        transform.append(node('a:ext', {'cx': cx, 'cy': cy}))
        shape.append(transform)
        geometry = node('a:prstGeom', {'prst': 'rect'})
        geometry.append(node('a:avLst'))
        shape.append(geometry)
        picture.append(shape)
        data.append(picture)
        graphic.append(data)
        inline.append(graphic)
        drawing.append(inline)
        run.append(drawing)
        return run

    def add_figures(self, figures):
        paragraph = self.paragraph('')
        ppr = child(paragraph, 'w:pPr')
        value(ppr, 'w:jc', 'center')
        child(ppr, 'w:ind').set(q('w:firstLine'), '0')
        value(ppr, 'w:keepNext', 1)
        captions = []
        for index, (caption, filename) in enumerate(figures):
            path = HERE / filename
            with Image.open(path) as im:
                width, height = im.size
            scale = min((208 if len(figures) == 2 else 445) / width, 430 / height)
            if index:
                space = self.paragraph('   ').find('w:r', NS)
                paragraph.append(space)
            paragraph.append(self.image_run(path, width * scale, height * scale, caption))
            caption = re.sub(r'^Figura\s+3\.2-[\da-z]+\s+—\s*', '', caption)
            captions.append(('À esquerda: ' if index == 0 else 'À direita: ') + caption if len(figures) == 2 else caption)
        self.body.append(paragraph)
        self.add_caption(' '.join(captions))
        self.body.append(self.paragraph(''))

    def fill(self, markdown):
        start = self.original.index(self.heading[1])
        end = self.original.index(self.heading[2])
        original_stage = self.original[start:end]
        for element in list(self.body):
            self.body.remove(element)
        for element in original_stage:
            self.body.append(deepcopy(element))
        markdown = markdown.replace('A implementação está na pasta', 'As listagens reproduzem trechos do código, com quebras de linha adaptadas à largura da página. A implementação está na pasta')
        lines = markdown.split('---PAGE---\n\n# Declaração de horas')[0].splitlines()
        i = 0
        while i < len(lines):
            line = lines[i].strip()
            if not line or line == '---PAGE---':
                i += 1
                continue
            if line.startswith('#'):
                self.heading_paragraph(line.lstrip('#').strip())
                i += 1
            elif line.startswith('```'):
                language = line[3:].strip()
                code = []
                i += 1
                while i < len(lines) and not lines[i].startswith('```'):
                    code.append(lines[i])
                    i += 1
                self.add_code(code, language)
                i += 1
            elif line.startswith('|'):
                rows = []
                while i < len(lines) and lines[i].strip().startswith('|'):
                    cells = [c.strip() for c in lines[i].strip().strip('|').split('|')]
                    if not all(re.fullmatch(r':?-+:?', c) for c in cells):
                        rows.append(cells)
                    i += 1
                self.add_table(rows)
            elif line.startswith('!['):
                figures = []
                while i < len(lines) and lines[i].startswith('![') and len(figures) < 2:
                    figures.append(re.fullmatch(r'!\[(.*)\]\((.*)\)', lines[i].strip()).groups())
                    i += 1
                self.add_figures(figures)
            else:
                content = [line]
                i += 1
                while i < len(lines) and lines[i].strip() and not lines[i].startswith(('#', '```', '|', '![', '---PAGE---')):
                    content.append(lines[i].strip())
                    i += 1
                joined = ' '.join(content)
                if joined.startswith('Listagem '):
                    title, *explanation = re.split(r'(?<=\.)\s+', joined, maxsplit=1)
                    self.add_caption(title)
                    if explanation:
                        self.body.append(self.paragraph(explanation[0]))
                else:
                    self.add_body(joined)
        # Tornar explícitas as fontes herdadas mantém a aparência ao copiar/importar.
        for paragraph in self.body.findall('.//w:p', NS):
            base = self.effective_run(paragraph)
            for run in paragraph.findall('w:r', NS):
                rpr = run.find('w:rPr', NS)
                if rpr is None:
                    rpr = node('w:rPr')
                    run.insert(0, rpr)
                for prop in base:
                    if rpr.find(prop.tag) is None:
                        rpr.append(deepcopy(prop))
        self.body.append(self.sect)

    def save(self, target):
        used = set(self.doc.xpath('//@r:embed', namespaces=NS))
        retained_media = set()
        for rel in list(self.rels):
            if rel.get('Type') == NS['r'] + '/image':
                if rel.get('Id') not in used:
                    self.rels.remove(rel)
                else:
                    retained_media.add('word/' + rel.get('Target').lstrip('/'))
        for name in list(self.parts):
            if name.startswith('word/media/') and name not in retained_media:
                del self.parts[name]
        for name, tree in [('word/document.xml', self.doc), ('word/styles.xml', self.styles),
                           ('word/numbering.xml', self.numbers), ('word/_rels/document.xml.rels', self.rels),
                           ('[Content_Types].xml', self.types)]:
            self.parts[name] = serialized(tree)
        with ZipFile(target, 'w', ZIP_DEFLATED) as package:
            for name, data in self.parts.items():
                package.writestr(name, data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--modelo', type=Path, required=True)
    args = parser.parse_args()
    builder = Builder(args.modelo)
    builder.fill((HERE / 'ETAPA_3.md').read_text())
    target = HERE / 'ETAPA_3_FORMATADA.docx'
    builder.save(target)
    print(target)
    print('SHA256 do modelo:', hashlib.sha256(args.modelo.read_bytes()).hexdigest())


if __name__ == '__main__':
    main()
