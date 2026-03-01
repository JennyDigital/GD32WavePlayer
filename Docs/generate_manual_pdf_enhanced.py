#!/usr/bin/env python3
"""
Generate comprehensive Audio Engine Manual as a professional A4 PDF.
Enhanced version with proper GitHub-style syntax highlighting using Pygments.
"""

import re
from pathlib import Path
from datetime import datetime
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import cm
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.enums import TA_LEFT, TA_CENTER, TA_JUSTIFY
from reportlab.platypus import (
    SimpleDocTemplate, Paragraph, Spacer, PageBreak, Table, TableStyle,
    Image, Preformatted
)
from reportlab.lib import colors
from reportlab.pdfgen import canvas
from reportlab.lib.colors import HexColor
from reportlab.graphics import renderPDF
from svglib.svglib import svg2rlg
from pygments import highlight
from pygments.lexers import CLexer, BashLexer, PythonLexer, get_lexer_by_name
from pygments.formatters import HtmlFormatter
from pygments.token import Token
import html
import os
import subprocess

BASE_DIR = Path(__file__).resolve().parent

# GitHub-style colors
COLOR_KEYWORD = HexColor('#d73a49')
COLOR_STRING = HexColor('#032f62')
COLOR_COMMENT = HexColor('#6a737d')
COLOR_FUNCTION = HexColor('#6f42c1')
COLOR_NUMBER = HexColor('#005cc5')
COLOR_BG = HexColor('#f6f8fa')
COLOR_TEXT = HexColor('#24292e')
COLOR_HEADING = HexColor('#0366d6')


class NumberedCanvas(canvas.Canvas):
    """Custom canvas with page numbers and headers."""
    
    def __init__(self, *args, **kwargs):
        canvas.Canvas.__init__(self, *args, **kwargs)
        self._saved_page_states = []

    def showPage(self):
        self._saved_page_states.append(dict(self.__dict__))
        self._startPage()

    def save(self):
        num_pages = len(self._saved_page_states)
        for state in self._saved_page_states:
            self.__dict__.update(state)
            self.draw_page_number(num_pages)
            canvas.Canvas.showPage(self)
        canvas.Canvas.save(self)

    def draw_page_number(self, page_count):
        page_num = self._pageNumber
        if page_num > 1:  # Skip title page
            # Footer
            self.setFont("Helvetica", 9)
            self.setFillColor(colors.grey)
            self.drawRightString(
                A4[0] - 2*cm, 1.5*cm,
                f"Page {page_num - 1} of {page_count - 1}"
            )
            # Header
            self.drawString(2*cm, A4[1] - 1.5*cm, "Audio Engine User Manual")
            self.line(2*cm, A4[1] - 1.6*cm, A4[0] - 2*cm, A4[1] - 1.6*cm)


def create_styles():
    """Create custom paragraph styles."""
    styles = getSampleStyleSheet()
    
    styles.add(ParagraphStyle(
        name='CustomTitle',
        parent=styles['Title'],
        fontSize=28,
        textColor=HexColor('#1a1a1a'),
        spaceAfter=30,
        alignment=TA_CENTER,
        fontName='Helvetica-Bold'
    ))
    
    styles.add(ParagraphStyle(
        name='CustomHeading1',
        parent=styles['Heading1'],
        fontSize=18,
        textColor=COLOR_HEADING,
        spaceAfter=12,
        spaceBefore=24,
        fontName='Helvetica-Bold',
        keepWithNext=True
    ))
    
    styles.add(ParagraphStyle(
        name='CustomHeading2',
        parent=styles['Heading2'],
        fontSize=14,
        textColor=COLOR_HEADING,
        spaceAfter=10,
        spaceBefore=20,
        fontName='Helvetica-Bold',
        keepWithNext=True
    ))
    
    styles.add(ParagraphStyle(
        name='CustomHeading3',
        parent=styles['Heading3'],
        fontSize=12,
        textColor=COLOR_TEXT,
        spaceAfter=8,
        spaceBefore=16,
        fontName='Helvetica-Bold',
        keepWithNext=True
    ))
    
    styles.add(ParagraphStyle(
        name='CustomBody',
        parent=styles['BodyText'],
        fontSize=10,
        textColor=COLOR_TEXT,
        spaceAfter=6,
        alignment=TA_JUSTIFY,
        fontName='Helvetica'
    ))
    
    styles.add(ParagraphStyle(
        name='CodeBlock',
        parent=styles['Code'],
        fontSize=7,
        textColor=COLOR_TEXT,
        backColor=COLOR_BG,
        borderColor=HexColor('#e1e4e8'),
        borderWidth=1,
        borderPadding=8,
        fontName='Courier',
        leftIndent=10,
        rightIndent=10,
        spaceAfter=12,
        spaceBefore=14,
        leading=8,
        wordWrap='CJK'
    ))
    
    styles.add(ParagraphStyle(
        name='BulletList',
        parent=styles['BodyText'],
        fontSize=10,
        textColor=COLOR_TEXT,
        leftIndent=20,
        spaceAfter=4,
        fontName='Helvetica'
    ))
    
    return styles


def parse_markdown_manual(md_file):
    """Parse the markdown manual and extract sections."""
    with open(md_file, 'r', encoding='utf-8') as f:
        content = f.read()
    
    sections = []
    current_section = {'title': '', 'level': 0, 'content': []}
    
    lines = content.split('\n')
    i = 0
    in_code_block = False
    code_block = []
    code_lang = 'c'
    
    while i < len(lines):
        line = lines[i]
        
        # Code blocks
        if line.strip().startswith('```'):
            if not in_code_block:
                in_code_block = True
                code_lang = line.strip()[3:] or 'c'
                code_block = []
            else:
                in_code_block = False
                current_section['content'].append({
                    'type': 'code',
                    'content': '\n'.join(code_block),
                    'language': code_lang
                })
                code_block = []
            i += 1
            continue
        
        if in_code_block:
            code_block.append(line)
            i += 1
            continue
        
        # Headings
        heading_match = re.match(r'^(#{1,6})\s+(.+)$', line)
        if heading_match:
            if current_section['title']:
                sections.append(current_section)
            level = len(heading_match.group(1))
            title = heading_match.group(2)
            current_section = {'title': title, 'level': level, 'content': []}
            i += 1
            continue
        
        # Tables
        if '|' in line and i + 1 < len(lines) and '---' in lines[i + 1]:
            table_lines = [line]
            i += 1
            table_lines.append(lines[i])
            i += 1
            while i < len(lines) and '|' in lines[i] and lines[i].strip():
                table_lines.append(lines[i])
                i += 1
            current_section['content'].append({
                'type': 'table',
                'content': table_lines
            })
            continue
        
        # Bullet lists
        if re.match(r'^\s*[-*]\s+', line):
            list_items = []
            while i < len(lines) and re.match(r'^\s*[-*]\s+', lines[i]):
                list_items.append(re.sub(r'^\s*[-*]\s+', '', lines[i]))
                i += 1
            current_section['content'].append({
                'type': 'list',
                'content': list_items
            })
            continue
        
        # Images
        img_match = re.match(r'!\[([^\]]*)\]\(([^\)]+)\)', line)
        if img_match:
            alt_text = img_match.group(1)
            img_path = img_match.group(2)
            current_section['content'].append({
                'type': 'image',
                'alt': alt_text,
                'path': img_path
            })
            i += 1
            continue
        
        # Regular paragraphs
        if line.strip():
            current_section['content'].append({
                'type': 'paragraph',
                'content': line
            })
        
        i += 1
    
    if current_section['title']:
        sections.append(current_section)
    
    return sections


def parse_table(table_lines):
    """Parse markdown table."""
    headers = [cell.strip() for cell in table_lines[0].split('|') if cell.strip()]
    rows = []
    for line in table_lines[2:]:
        cells = [cell.strip() for cell in line.split('|') if cell.strip()]
        if cells:
            rows.append(cells)
    return headers, rows


def create_table_flowable(headers, rows):
    """Create formatted table with proper cell wrapping."""
    # Convert cells to Paragraphs for better text wrapping
    table_style = ParagraphStyle('TableCell', fontSize=8, fontName='Helvetica')
    header_style = ParagraphStyle('TableHeader', fontSize=9, fontName='Helvetica-Bold')
    
    # Process headers
    header_paras = [Paragraph(process_inline_code(h), header_style) for h in headers]
    
    # Process rows
    row_paras = []
    for row in rows:
        row_paras.append([Paragraph(process_inline_code(cell), table_style) for cell in row])
    
    data = [header_paras] + row_paras
    
    # Dynamic column widths based on header count
    available_width = A4[0] - 4*cm  # Account for margins
    col_widths = [available_width / len(headers) for _ in headers]
    
    table = Table(data, colWidths=col_widths)
    table.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), HexColor('#6fa3d4')),  # Lighter blue for better print contrast
        ('TEXTCOLOR', (0, 0), (-1, 0), colors.white),
        ('ALIGN', (0, 0), (-1, -1), 'LEFT'),
        ('FONTNAME', (0, 0), (-1, 0), 'Helvetica-Bold'),
        ('FONTSIZE', (0, 0), (-1, 0), 9),
        ('FONTNAME', (0, 1), (-1, -1), 'Helvetica'),
        ('FONTSIZE', (0, 1), (-1, -1), 8),
        ('GRID', (0, 0), (-1, -1), 0.5, HexColor('#e1e4e8')),
        ('ROWBACKGROUNDS', (0, 1), (-1, -1), [colors.white, COLOR_BG]),
        ('VALIGN', (0, 0), (-1, -1), 'TOP'),
        ('LEFTPADDING', (0, 0), (-1, -1), 6),
        ('RIGHTPADDING', (0, 0), (-1, -1), 6),
        ('TOPPADDING', (0, 0), (-1, -1), 4),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 4),
    ]))
    
    return table


def process_inline_code(text):
    """Convert inline markdown to styled text with proper escaping."""
    # Escape HTML characters first
    text = html.escape(text, quote=False)
    # Then apply formatting
    text = re.sub(r'`([^`]+)`', 
                 r'<font face="Courier" color="#d73a49" backColor="#f6f8fa"><sub> </sub>\1<sub> </sub></font>', 
                 text)
    text = re.sub(r'\*\*([^*]+)\*\*', r'<b>\1</b>', text)
    text = re.sub(r'\*([^*]+)\*', r'<i>\1</i>', text)
    return text


def colorize_code(code, language='c'):
    """Apply syntax highlighting using Pygments and convert to ReportLab markup."""
    try:
        lexer = get_lexer_by_name(language, stripall=True)
    except:
        lexer = CLexer()
    
    tokens = list(lexer.get_tokens(code))
    output_lines = []
    current_line = []
    
    for ttype, value in tokens:
        # Escape XML special characters
        value = html.escape(value)
        
        # Apply colors based on token type
        if ttype in Token.Keyword:
            current_line.append(f'<font color="#d73a49"><b>{value}</b></font>')
        elif ttype in Token.String:
            current_line.append(f'<font color="#032f62">{value}</font>')
        elif ttype in Token.Comment:
            current_line.append(f'<font color="#6a737d"><i>{value}</i></font>')
        elif ttype in Token.Name.Function:
            current_line.append(f'<font color="#6f42c1">{value}</font>')
        elif ttype in Token.Number or ttype in Token.Literal.Number:
            current_line.append(f'<font color="#005cc5">{value}</font>')
        elif ttype in Token.Operator:
            current_line.append(f'<font color="#d73a49">{value}</font>')
        elif ttype in Token.Name.Builtin:
            current_line.append(f'<font color="#005cc5">{value}</font>')
        else:
            current_line.append(value)
        
        # Handle newlines
        if '\n' in value:
            parts = value.split('\n')
            for i, part in enumerate(parts[:-1]):
                output_lines.append(''.join(current_line))
                current_line = []
            if parts[-1]:
                current_line.append(parts[-1])
    
    if current_line:
        output_lines.append(''.join(current_line))
    
    return '\n'.join(output_lines)


def convert_svg_to_png(svg_path, png_path, width=None):
    """Convert SVG to PNG using cairosvg if available, otherwise return SVG path."""
    try:
        import cairosvg
        cairosvg.svg2png(url=svg_path, write_to=png_path, output_width=width)
        return png_path
    except ImportError:
        # If cairosvg not available, try using rsvg-convert (librsvg)
        try:
            cmd = ['rsvg-convert', svg_path, '-o', png_path]
            if width:
                cmd.extend(['-w', str(width)])
            subprocess.run(cmd, check=True, capture_output=True)
            return png_path
        except (subprocess.CalledProcessError, FileNotFoundError):
            # If conversion fails, just return the SVG path
            print(f"Warning: Could not convert {svg_path} to PNG. SVG will be embedded directly.")
            return svg_path


def parse_table(table_lines):
    """Parse markdown table."""
    headers = [cell.strip() for cell in table_lines[0].split('|') if cell.strip()]
    rows = []
    for line in table_lines[2:]:
        cells = [cell.strip() for cell in line.split('|') if cell.strip()]
        if cells:
            rows.append(cells)
    return headers, rows


def build_pdf_content(sections, styles):
    """Build PDF content."""
    story = []
    
    # Title page
    story.append(Spacer(1, 3*cm))
    story.append(Paragraph("Audio Engine", styles['CustomTitle']))
    story.append(Paragraph("User Manual", styles['CustomTitle']))
    story.append(Spacer(1, 1*cm))
    story.append(Paragraph(
        "GD32 DSP Audio Playback System<br/>for microcontrollers with I2S support<br/>Version 2.0",
        ParagraphStyle('subtitle', parent=styles['CustomBody'], 
                      fontSize=14, alignment=TA_CENTER, textColor=HexColor('#586069'))
    ))
    story.append(Spacer(1, 0.5*cm))
    
    # Feature badges
    story.append(Paragraph(
        '<font face="Courier" size="8" color="#586069">'
        '8-bit | 16-bit | Mono | Stereo | Runtime DSP | No FPU'
        '</font>',
        ParagraphStyle('badges', parent=styles['CustomBody'], alignment=TA_CENTER)
    ))
    
    story.append(Spacer(1, 2*cm))
    story.append(Paragraph(
        f"Generated: {datetime.now().strftime('%B %d, %Y')}",
        ParagraphStyle('date', parent=styles['CustomBody'], 
                      alignment=TA_CENTER, textColor=HexColor('#6a737d'))
    ))
    story.append(PageBreak())
    
    # Process sections
    for section in sections:
        level = section['level']
        title = section['title']
        
        if 'Table of Contents' in title or 'table of contents' in title.lower():
            continue
        
        # Add page break before Architecture section
        if title == 'Architecture' and level == 2:
            story.append(PageBreak())
        
        # Add heading
        style_name = ['CustomHeading1', 'CustomHeading2', 'CustomHeading3'][min(level-1, 2)]
        story.append(Paragraph(title, styles[style_name]))
        
        # Content
        for item in section['content']:
            if item['type'] == 'paragraph':
                text = process_inline_code(item['content'])
                story.append(Paragraph(text, styles['CustomBody']))
            
            elif item['type'] == 'code':
                # Use Pygments syntax highlighting
                code = item['content']
                language = item.get('language', 'c')
                
                # Split long lines and limit total lines
                code_lines = code.split('\n')
                formatted_lines = []
                max_line_length = 85  # Characters per line
                max_lines = 50  # Maximum lines per code block
                
                for line in code_lines[:max_lines]:
                    # Break long lines
                    while len(line) > max_line_length:
                        formatted_lines.append(line[:max_line_length])
                        line = '  ' + line[max_line_length:]  # Indent continuation
                    formatted_lines.append(line)
                
                if len(code_lines) > max_lines:
                    formatted_lines.append('... (truncated)')
                
                code_text = '\n'.join(formatted_lines)
                
                # Apply syntax highlighting
                try:
                    highlighted_code = colorize_code(code_text, language)
                    # Replace newlines with <br/> for Paragraph rendering
                    highlighted_code = highlighted_code.replace('\n', '<br/>')
                    code_para = Paragraph(
                        f'<font face="Courier" size="7">{highlighted_code}</font>',
                        styles['CodeBlock']
                    )
                    story.append(code_para)
                except Exception as e:
                    # Fallback to plain preformatted
                    story.append(Preformatted(code_text, styles['CodeBlock']))
            
            elif item['type'] == 'table':
                headers, rows = parse_table(item['content'])
                table = create_table_flowable(headers, rows)
                story.append(table)
                story.append(Spacer(1, 0.2*cm))
            
            elif item['type'] == 'list':
                for list_item in item['content']:
                    text = process_inline_code(list_item)
                    bullet_para = Paragraph(f'• {text}', styles['BulletList'])
                    story.append(bullet_para)
                story.append(Spacer(1, 0.2*cm))
            
            elif item['type'] == 'image':
                # Handle images (SVG or PNG)
                img_path = item['path']
                alt_text = item.get('alt', '')
                
                # Try to load the image
                try:
                    if os.path.exists(img_path):
                        # Add caption if alt text exists
                        if alt_text:
                            story.append(Spacer(1, 0.3*cm))
                            story.append(Paragraph(
                                f"<b>{alt_text}</b>",
                                ParagraphStyle('caption', parent=styles['CustomBody'],
                                             fontSize=9, alignment=TA_CENTER, 
                                             textColor=HexColor('#586069'), spaceAfter=6)
                            ))
                        
                        # Handle SVG with svglib
                        if img_path.endswith('.svg'):
                            drawing = svg2rlg(img_path)
                            if drawing:
                                # Get dimensions
                                width = drawing.width if isinstance(drawing.width, (int, float)) else (drawing.width[0] if isinstance(drawing.width, list) and drawing.width else 600)
                                height = drawing.height if isinstance(drawing.height, (int, float)) else (drawing.height[0] if isinstance(drawing.height, list) and drawing.height else 700)
                                
                                # Scale to fit page (increase vertical space for block diagram)
                                target_width = 13*cm
                                target_height = 16*cm  # Reduced to fit on same page as heading
                                scale_x = target_width / width
                                scale_y = target_height / height
                                scale = min(scale_x, scale_y)
                                
                                drawing.width = width * scale
                                drawing.height = height * scale
                                drawing.scale(scale, scale)
                                
                                # Center the diagram by wrapping in a table
                                from reportlab.platypus import KeepTogether, Table as TablePlat
                                centered_table = TablePlat([[drawing]], colWidths=[None])
                                centered_table.setStyle(TableStyle([
                                    ('ALIGN', (0, 0), (-1, -1), 'CENTER'),
                                    ('VALIGN', (0, 0), (-1, -1), 'MIDDLE'),
                                ]))
                                story.append(KeepTogether([centered_table]))
                        else:
                            # Handle PNG/JPG
                            img = Image(img_path, width=15*cm, height=15*cm, kind='proportional')
                            story.append(img)
                        
                        story.append(Spacer(1, 0.5*cm))
                except Exception as e:
                    print(f"Warning: Could not load image {img_path}: {e}")
        
        # Add filter graph at end of Filter Configuration section
        if 'Air Effect' in title and 'High-Shelf' in title and level == 3:
            try:
                # Page break to give graph its own page
                story.append(PageBreak())
                story.append(Spacer(1, 0.3*cm))
                
                # Full-page image with embedded caption
                # A4 with 2cm margins leaves 17cm width and 25.7cm height
                img = Image(str(BASE_DIR / 'filter_characteristics_enhanced.png'), 
                          width=17*cm, height=22*cm)
                story.append(img)
                
                # Page break after to separate from next section
                story.append(PageBreak())
            except:
                pass
    
    return story


def main():
    """Generate PDF manual."""
    print("=" * 70)
    print("Generating Enhanced Audio Engine Manual PDF")
    print("=" * 70)
    
    pdf_file = str(BASE_DIR / "Audio_Engine_Manual.pdf")
    doc = SimpleDocTemplate(
        pdf_file,
        pagesize=A4,
        leftMargin=2*cm,
        rightMargin=2*cm,
        topMargin=2.5*cm,
        bottomMargin=2.5*cm
    )
    
    styles = create_styles()
    
    print("Parsing AUDIO_ENGINE_MANUAL.md...")
    sections = parse_markdown_manual(str(BASE_DIR / 'AUDIO_ENGINE_MANUAL.md'))
    print(f"  Found {len(sections)} sections")
    
    print("Building PDF content...")
    story = build_pdf_content(sections, styles)
    print(f"  Created {len(story)} flowable elements")
    
    print("Rendering PDF...")
    doc.build(story, canvasmaker=NumberedCanvas)
    
    print("\n" + "=" * 70)
    print("Enhanced PDF Manual Generated Successfully!")
    print("=" * 70)
    print(f"Output File:  {pdf_file}")
    print(f"Format:       A4 (210 × 297 mm)")
    print(f"Features:     - Professional formatting with color scheme")
    print(f"              - Embedded filter response graph (Figure 1)")
    print(f"              - Code blocks with monospace formatting")
    print(f"              - Tables with alternating row colors")
    print(f"              - Page numbers and headers")
    print(f"              - Inline code highlighting")
    print(f"Date:         {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print("=" * 70)


if __name__ == '__main__':
    main()
