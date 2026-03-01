#!/usr/bin/env python3
"""
Generate comprehensive Audio Engine Manual as a professional A4 PDF.
Includes syntax-highlighted code snippets, embedded graphs, and full formatting.
"""

import re
from pathlib import Path
from datetime import datetime
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import cm, inch
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.enums import TA_LEFT, TA_CENTER, TA_JUSTIFY
from reportlab.platypus import (
    SimpleDocTemplate, Paragraph, Spacer, PageBreak, Table, TableStyle,
    Image, KeepTogether, ListFlowable, ListItem
)
from reportlab.lib import colors
from reportlab.pdfgen import canvas
from reportlab.lib.colors import HexColor
from pygments import highlight
from pygments.lexers import CLexer, get_lexer_by_name
from pygments.formatters import HtmlFormatter
from pygments.styles import get_style_by_name
import markdown

BASE_DIR = Path(__file__).resolve().parent

# GitHub-style color scheme for code
GITHUB_COLORS = {
    'keyword': '#d73a49',      # Red
    'string': '#032f62',       # Blue
    'comment': '#6a737d',      # Gray
    'function': '#6f42c1',     # Purple
    'number': '#005cc5',       # Blue
    'operator': '#d73a49',     # Red
    'background': '#f6f8fa',   # Light gray
    'text': '#24292e',         # Dark gray
}

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
            # Footer with page number
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
    
    # Title
    styles.add(ParagraphStyle(
        name='CustomTitle',
        parent=styles['Title'],
        fontSize=28,
        textColor=HexColor('#1a1a1a'),
        spaceAfter=30,
        alignment=TA_CENTER,
        fontName='Helvetica-Bold'
    ))
    
    # Heading 1
    styles.add(ParagraphStyle(
        name='CustomHeading1',
        parent=styles['Heading1'],
        fontSize=18,
        textColor=HexColor('#0366d6'),
        spaceAfter=12,
        spaceBefore=20,
        fontName='Helvetica-Bold',
        keepWithNext=True
    ))
    
    # Heading 2
    styles.add(ParagraphStyle(
        name='CustomHeading2',
        parent=styles['Heading2'],
        fontSize=14,
        textColor=HexColor('#0366d6'),
        spaceAfter=10,
        spaceBefore=16,
        fontName='Helvetica-Bold',
        keepWithNext=True
    ))
    
    # Heading 3
    styles.add(ParagraphStyle(
        name='CustomHeading3',
        parent=styles['Heading3'],
        fontSize=12,
        textColor=HexColor('#24292e'),
        spaceAfter=8,
        spaceBefore=12,
        fontName='Helvetica-Bold',
        keepWithNext=True
    ))
    
    # Body text
    styles.add(ParagraphStyle(
        name='CustomBody',
        parent=styles['BodyText'],
        fontSize=10,
        textColor=HexColor('#24292e'),
        spaceAfter=6,
        alignment=TA_JUSTIFY,
        fontName='Helvetica'
    ))
    
    # Code block
    styles.add(ParagraphStyle(
        name='CodeBlock',
        parent=styles['Code'],
        fontSize=9,
        textColor=HexColor('#24292e'),
        backColor=HexColor('#f6f8fa'),
        borderColor=HexColor('#e1e4e8'),
        borderWidth=1,
        borderPadding=8,
        fontName='Courier',
        leftIndent=10,
        rightIndent=10,
        spaceAfter=12,
        spaceBefore=6
    ))
    
    # Inline code
    styles.add(ParagraphStyle(
        name='InlineCode',
        parent=styles['Normal'],
        fontSize=9,
        textColor=HexColor('#d73a49'),
        backColor=HexColor('#f6f8fa'),
        fontName='Courier'
    ))
    
    # Table header
    styles.add(ParagraphStyle(
        name='TableHeader',
        parent=styles['Normal'],
        fontSize=10,
        textColor=colors.white,
        fontName='Helvetica-Bold',
        alignment=TA_CENTER
    ))
    
    # Bullet list
    styles.add(ParagraphStyle(
        name='BulletList',
        parent=styles['BodyText'],
        fontSize=10,
        textColor=HexColor('#24292e'),
        leftIndent=20,
        spaceAfter=4,
        fontName='Helvetica'
    ))
    
    return styles


def syntax_highlight_code(code, language='c'):
    """Apply GitHub-style syntax highlighting to code with proper indentation."""
    from pygments import highlight
    from pygments.lexers import get_lexer_by_name
    from pygments.token import Token
    
    try:
        lexer = get_lexer_by_name(language, stripall=False)
    except:
        lexer = get_lexer_by_name('c', stripall=False)
    
    # Token color mapping (GitHub style)
    token_colors = {
        Token.Keyword: '#d73a49',
        Token.Keyword.Type: '#d73a49',
        Token.Keyword.Namespace: '#d73a49',
        Token.String: '#032f62',
        Token.String.Char: '#032f62',
        Token.Comment: '#6a737d',
        Token.Comment.Single: '#6a737d',
        Token.Comment.Multiline: '#6a737d',
        Token.Name.Function: '#6f42c1',
        Token.Name.Class: '#6f42c1',
        Token.Number: '#005cc5',
        Token.Operator: '#d73a49',
        Token.Punctuation: '#24292e',
        Token.Name.Builtin: '#005cc5',
        Token.Name: '#24292e',
    }
    
    # Tokenize and colorize
    tokens = lexer.get_tokens(code)
    result = []
    
    for token_type, token_value in tokens:
        # Preserve whitespace and indentation
        token_value = token_value.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')
        token_value = token_value.replace(' ', '&nbsp;')  # Preserve spaces
        token_value = token_value.replace('\n', '<br/>')
        token_value = token_value.replace('\t', '&nbsp;&nbsp;&nbsp;&nbsp;')  # Tab to 4 spaces
        
        # Find matching color
        color = '#24292e'  # Default text color
        for ttype, tcolor in token_colors.items():
            if token_type in ttype:
                color = tcolor
                break
        
        if token_value:
            result.append(f'<font color="{color}">{token_value}</font>')
    
    return ''.join(result)


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
            current_section = {'title': title, 'level': level, 'content': [], 'line': i}
            i += 1
            continue
        
        # Tables
        if '|' in line and i + 1 < len(lines) and '---' in lines[i + 1]:
            table_lines = [line]
            i += 1
            # Separator line
            table_lines.append(lines[i])
            i += 1
            # Data rows
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
        
        # Skip markdown image references (they'll be inserted programmatically)
        if re.match(r'^\s*!\[.*\]\(.*\)\s*$', line):
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
    """Parse markdown table into data structure."""
    headers = [cell.strip() for cell in table_lines[0].split('|') if cell.strip()]
    rows = []
    for line in table_lines[2:]:  # Skip header and separator
        cells = [cell.strip() for cell in line.split('|') if cell.strip()]
        if cells:
            rows.append(cells)
    return headers, rows


def create_table_flowable(headers, rows, styles):
    """Create a formatted table flowable with wrapped cell text."""
    table_style = ParagraphStyle('TableCell', fontSize=8, fontName='Helvetica')
    header_style = ParagraphStyle('TableHeader', fontSize=9, fontName='Helvetica-Bold')

    header_paras = [Paragraph(process_inline_code(h), header_style) for h in headers]
    row_paras = []
    for row in rows:
        row_paras.append([Paragraph(process_inline_code(cell), table_style) for cell in row])

    data = [header_paras] + row_paras

    # Calculate column widths based on page margins
    available_width = A4[0] - 4*cm
    col_widths = [available_width / len(headers) for _ in headers]

    table = Table(data, colWidths=col_widths)
    table.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), HexColor('#0366d6')),
        ('TEXTCOLOR', (0, 0), (-1, 0), colors.white),
        ('ALIGN', (0, 0), (-1, -1), 'LEFT'),
        ('FONTNAME', (0, 0), (-1, 0), 'Helvetica-Bold'),
        ('FONTSIZE', (0, 0), (-1, 0), 10),
        ('FONTNAME', (0, 1), (-1, -1), 'Helvetica'),
        ('FONTSIZE', (0, 1), (-1, -1), 9),
        ('GRID', (0, 0), (-1, -1), 0.5, HexColor('#e1e4e8')),
        ('ROWBACKGROUNDS', (0, 1), (-1, -1), [colors.white, HexColor('#f6f8fa')]),
        ('VALIGN', (0, 0), (-1, -1), 'TOP'),
        ('LEFTPADDING', (0, 0), (-1, -1), 8),
        ('RIGHTPADDING', (0, 0), (-1, -1), 8),
        ('TOPPADDING', (0, 0), (-1, -1), 6),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 6),
    ]))
    
    return table


def process_inline_code(text):
    """Convert inline code markers to styled text."""
    # Replace `code` with styled inline code
    text = re.sub(r'`([^`]+)`', 
                 r'<font face="Courier" color="#d73a49" backColor="#f6f8fa"> \1 </font>', 
                 text)
    # Bold
    text = re.sub(r'\*\*([^*]+)\*\*', r'<b>\1</b>', text)
    # Italic
    text = re.sub(r'\*([^*]+)\*', r'<i>\1</i>', text)
    
    return text


def build_pdf_content(sections, styles):
    """Build PDF content from parsed sections."""
    story = []
    
    # Title page
    story.append(Spacer(1, 3*cm))
    story.append(Paragraph("Audio Engine", styles['CustomTitle']))
    story.append(Paragraph("User Manual", styles['CustomTitle']))
    story.append(Spacer(1, 1*cm))
    story.append(Paragraph(
        "STM32G474 DSP Audio Playback System<br/>Version 2.0",
        ParagraphStyle('subtitle', parent=styles['CustomBody'], 
                      fontSize=14, alignment=TA_CENTER, textColor=HexColor('#586069'))
    ))
    story.append(Spacer(1, 2*cm))
    story.append(Paragraph(
        f"Generated: {datetime.now().strftime('%B %d, %Y')}",
        ParagraphStyle('date', parent=styles['CustomBody'], 
                      alignment=TA_CENTER, textColor=HexColor('#6a737d'))
    ))
    story.append(PageBreak())
    
    # Process each section
    for section in sections:
        level = section['level']
        title = section['title']
        
        # Skip TOC section - we'll generate our own
        if 'Table of Contents' in title or 'table of contents' in title.lower():
            continue
        
        # Add heading
        if level == 1:
            style_name = 'CustomHeading1'
        elif level == 2:
            style_name = 'CustomHeading2'
        else:
            style_name = 'CustomHeading3'

        if title.strip() == '8-bit LPF Aggressiveness Levels':
            story.append(PageBreak())
        
        story.append(Paragraph(title, styles[style_name]))
        
        # Process content
        for item in section['content']:
            if item['type'] == 'paragraph':
                text = process_inline_code(item['content'])
                story.append(Paragraph(text, styles['CustomBody']))
            
            elif item['type'] == 'code':
                # Add spacer before code block to prevent overlap
                story.append(Spacer(1, 0.3*cm))
                code = item['content']
                highlighted = syntax_highlight_code(code, item.get('language', 'c'))
                # Wrap in pre tags to preserve formatting
                code_para = Paragraph(
                    f'<pre><font face="Courier" size="9">{highlighted}</font></pre>',
                    styles['CodeBlock']
                )
                story.append(code_para)
                # Add spacer after code block
                story.append(Spacer(1, 0.2*cm))
            
            elif item['type'] == 'table':
                headers, rows = parse_table(item['content'])
                table = create_table_flowable(headers, rows, styles)
                story.append(table)
                story.append(Spacer(1, 0.3*cm))
            
            elif item['type'] == 'list':
                bullet_items = []
                for list_item in item['content']:
                    text = process_inline_code(list_item)
                    bullet_items.append(Paragraph(text, styles['BulletList']))
                story.append(ListFlowable(bullet_items, bulletType='bullet'))
        
        # Add system block diagram after Architecture section
        if 'System Block Diagram' in title and level == 3:
            import os
            svg_path = str(BASE_DIR / 'system_block_diagram.svg')
            if os.path.exists(svg_path):
                try:
                    from svglib.svglib import svg2rlg
                    from reportlab.graphics import renderPDF
                    
                    # Convert SVG to ReportLab drawing
                    drawing = svg2rlg(svg_path)
                    if drawing:
                        # Get original dimensions
                        orig_width = drawing.width
                        orig_height = drawing.height
                        
                        # Scale to fit page width (with margins)
                        target_width = 14*cm
                        scale_factor = target_width / orig_width
                        
                        drawing.width = target_width
                        drawing.height = orig_height * scale_factor
                        drawing.scale(scale_factor, scale_factor)
                        
                        story.append(Spacer(1, 0.5*cm))
                        story.append(drawing)
                        story.append(Spacer(1, 0.5*cm))
                    else:
                        story.append(Paragraph(
                            "<i>[System block diagram could not be rendered]</i>",
                            styles['CustomBody']
                        ))
                except ImportError:
                    story.append(Paragraph(
                        "<i>[svglib not installed - install with: pip install svglib]</i>",
                        styles['CustomBody']
                    ))
                except Exception as e:
                    story.append(Paragraph(
                        f"<i>[Error loading diagram: {str(e)}]</i>",
                        styles['CustomBody']
                    ))
            else:
                story.append(Paragraph(
                    f"<i>[System block diagram file not found: {svg_path}]</i>",
                    styles['CustomBody']
                ))
        
        # Add filter graph after filter configuration section
        if 'Filter Configuration' in title and level == 2:
            try:
                story.append(Spacer(1, 0.5*cm))
                story.append(Paragraph(
                    "<b>Figure 1:</b> Comprehensive Filter Frequency Response Analysis",
                    ParagraphStyle('caption', parent=styles['CustomBody'],
                                 fontSize=10, alignment=TA_CENTER, 
                                 textColor=HexColor('#586069'))
                ))
                img = Image(str(BASE_DIR / 'filter_characteristics_enhanced.png'), width=16*cm, height=12*cm)
                story.append(img)
                story.append(Spacer(1, 0.5*cm))
            except:
                pass  # Image not found, skip
    
    return story


def main():
    """Generate the PDF manual."""
    print("=" * 70)
    print("Generating Audio Engine Manual PDF")
    print("=" * 70)
    
    # Create PDF
    pdf_file = str(BASE_DIR / "Audio_Engine_Manual.pdf")
    doc = SimpleDocTemplate(
        pdf_file,
        pagesize=A4,
        leftMargin=2*cm,
        rightMargin=2*cm,
        topMargin=2.5*cm,
        bottomMargin=2.5*cm
    )
    
    # Create styles
    styles = create_styles()
    
    # Parse markdown
    print("Parsing AUDIO_ENGINE_MANUAL.md...")
    sections = parse_markdown_manual(str(BASE_DIR / 'AUDIO_ENGINE_MANUAL.md'))
    print(f"  Found {len(sections)} sections")
    
    # Build content
    print("Building PDF content...")
    story = build_pdf_content(sections, styles)
    print(f"  Created {len(story)} flowable elements")
    
    # Generate PDF
    print("Rendering PDF...")
    doc.build(story, canvasmaker=NumberedCanvas)
    
    print("\n" + "=" * 70)
    print("PDF Manual Generated Successfully!")
    print("=" * 70)
    print(f"Output File:  {pdf_file}")
    print(f"Format:       A4 (210 × 297 mm)")
    print(f"Features:     - Syntax-highlighted code snippets")
    print(f"              - Embedded filter response graphs")
    print(f"              - Professional formatting")
    print(f"              - Page numbers and headers")
    print(f"Date:         {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print("=" * 70)


if __name__ == '__main__':
    main()
