#!/usr/bin/env python3
"""Generate a coursework report DOCX from the current repository checkout.

This generator deliberately avoids claiming that tests passed: test outcomes must be
verified from a real CTest/CI run and are described as unverified unless supplied.
"""
from __future__ import annotations

import argparse
import os
import re
import sys
import zipfile
from pathlib import Path
from typing import Iterable

try:
    from docx import Document
    from docx.enum.section import WD_SECTION, WD_ORIENT
    from docx.enum.table import WD_CELL_VERTICAL_ALIGNMENT
    from docx.enum.text import WD_ALIGN_PARAGRAPH, WD_TAB_ALIGNMENT, WD_TAB_LEADER
    from docx.enum.style import WD_STYLE_TYPE
    from docx.shared import Cm, Pt, RGBColor
except ImportError as exc:
    raise SystemExit("Missing dependency. Install it with: python -m pip install python-docx") from exc


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = ROOT / "dist" / "Отчет_курсовая_ТРПО_DiskAnalyzer.docx"
ASSIGNMENT = ROOT / "docs" / "reporting" / "ASSIGNMENT.md"
REPORT_SPEC = ROOT / "docs" / "reporting" / "REPORT_SPECIFICATION.md"
SOURCE_EXTENSIONS = {".h", ".hpp", ".c", ".cc", ".cpp", ".cxx"}
EXCLUDED_PARTS = {".git", "build", "dist", ".venv", "venv", "__pycache__", "node_modules"}


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def markdown_quote_text(path: Path) -> str:
    """Extract the teacher's verbatim blockquote from ASSIGNMENT.md."""
    text = read_text(path)
    lines = text.splitlines()
    inside = False
    collected: list[str] = []
    for line in lines:
        if line.startswith("> "):
            inside = True
            collected.append(line[2:])
        elif inside and not line.strip():
            break
    if not collected:
        raise RuntimeError(f"Could not find quoted assignment in {path}")
    return "\n".join(collected)


def source_files() -> list[Path]:
    found: list[Path] = []
    for folder in (ROOT / "src", ROOT / "tests"):
        if not folder.exists():
            continue
        for path in folder.rglob("*"):
            if not path.is_file() or path.suffix.lower() not in SOURCE_EXTENSIONS:
                continue
            if any(part in EXCLUDED_PARTS for part in path.parts):
                continue
            found.append(path)
    # Include build/test configuration as source artifacts, not as C++ source.
    for candidate in (ROOT / "CMakeLists.txt", ROOT / ".github" / "workflows" / "build-release.yml"):
        if candidate.is_file():
            found.append(candidate)
    return sorted(set(found), key=lambda p: p.relative_to(ROOT).as_posix().lower())


def relative(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def configure_document(doc: Document) -> None:
    sec = doc.sections[0]
    sec.page_width = Cm(21)
    sec.page_height = Cm(29.7)
    sec.top_margin = Cm(2)
    sec.bottom_margin = Cm(2)
    sec.left_margin = Cm(2.5)
    sec.right_margin = Cm(1.5)
    styles = doc.styles
    from docx.oxml import OxmlElement
    from docx.oxml.ns import qn
    update_fields = OxmlElement("w:updateFields")
    update_fields.set(qn("w:val"), "true")
    doc.settings._element.append(update_fields)
    styles["Normal"].font.name = "Times New Roman"
    styles["Normal"].font.size = Pt(12)
    styles["Normal"].paragraph_format.space_after = Pt(5)
    for name in ("Title", "Heading 1", "Heading 2", "Heading 3"):
        styles[name].font.name = "Times New Roman"
    styles["Heading 1"].font.size = Pt(16)
    styles["Heading 1"].font.bold = True
    styles["Heading 2"].font.size = Pt(14)
    styles["Heading 3"].font.size = Pt(12)
    styles["Heading 1"].paragraph_format.keep_with_next = True
    styles["Heading 2"].paragraph_format.keep_with_next = True


def add_page_number(section) -> None:
    footer = section.footer.paragraphs[0]
    footer.alignment = WD_ALIGN_PARAGRAPH.CENTER
    run = footer.add_run()
    # Word PAGE field. LibreOffice and Microsoft Word resolve this on opening/updating.
    from docx.oxml import OxmlElement
    from docx.oxml.ns import qn
    fld = OxmlElement("w:fldSimple")
    fld.set(qn("w:instr"), "PAGE")
    run._r.addnext(fld)


def add_paragraph(doc: Document, text: str = "", *, bold: bool = False, italic: bool = False):
    p = doc.add_paragraph()
    r = p.add_run(text)
    r.bold = bold
    r.italic = italic
    return p


def add_bullets(doc: Document, items: Iterable[str]) -> None:
    for item in items:
        doc.add_paragraph(item, style="List Bullet")


def add_numbered(doc: Document, items: Iterable[str]) -> None:
    for item in items:
        doc.add_paragraph(item, style="List Number")


def add_table(doc: Document, headers: list[str], rows: list[list[str]], widths_cm: list[float] | None = None):
    table = doc.add_table(rows=1, cols=len(headers))
    table.style = "Table Grid"
    for i, label in enumerate(headers):
        cell = table.rows[0].cells[i]
        cell.text = label
        for run in cell.paragraphs[0].runs:
            run.bold = True
            run.font.name = "Times New Roman"
            run.font.size = Pt(10)
    for row in rows:
        cells = table.add_row().cells
        for i, value in enumerate(row):
            cells[i].text = str(value)
            cells[i].vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER
            for para in cells[i].paragraphs:
                para.paragraph_format.space_after = Pt(2)
                for run in para.runs:
                    run.font.name = "Times New Roman"
                    run.font.size = Pt(9)
    if widths_cm:
        for row in table.rows:
            for i, width in enumerate(widths_cm):
                row.cells[i].width = Cm(width)
    doc.add_paragraph()
    return table


def add_code_block(doc: Document, text: str, *, as_table: bool = False) -> None:
    if as_table:
        table = doc.add_table(rows=1, cols=1)
        table.style = "Table Grid"
        cell = table.cell(0, 0)
        cell.text = ""
        p = cell.paragraphs[0]
        p.paragraph_format.space_after = Pt(0)
        r = p.add_run(text.rstrip("\n"))
        r.font.name = "Courier New"
        r.font.size = Pt(7)
        r.font.color.rgb = RGBColor(30, 30, 30)
        cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.TOP
    else:
        for line in text.rstrip("\n").splitlines():
            p = doc.add_paragraph()
            p.paragraph_format.space_after = Pt(0)
            p.paragraph_format.left_indent = Cm(0.5)
            r = p.add_run(line if line else " ")
            r.font.name = "Courier New"
            r.font.size = Pt(9)


def _split_parameters(text: str) -> list[str]:
    """Split a C++ parameter list without splitting template arguments."""
    result: list[str] = []
    depth = 0
    start = 0
    for index, char in enumerate(text):
        if char in "<([{":
            depth += 1
        elif char in ">)]}":
            depth = max(0, depth - 1)
        elif char == "," and depth == 0:
            part = text[start:index].strip()
            if part and part != "void":
                result.append(part)
            start = index + 1
    part = text[start:].strip()
    if part and part != "void":
        result.append(part)
    return result


def _argument_description(argument: str) -> str:
    """Return the declared type, parameter name, and a concise role."""
    normalized = re.sub(r"\s+", " ", argument.strip())
    normalized = re.sub(r"\s*=\s*.*$", "", normalized).strip()
    match = re.search(r"([A-Za-z_]\w*)$", normalized)
    name = match.group(1) if match else normalized
    arg_type = normalized[:match.start()].strip() if match else "тип не определён"
    low = name.lower()
    if any(word in low for word in ("path", "directory", "folder", "root")): role = "путь/каталог для обработки"
    elif any(word in low for word in ("file", "filename")): role = "файл для обработки"
    elif any(word in low for word in ("size", "bytes", "count", "total")): role = "размер или количество"
    elif any(word in low for word in ("category", "type", "format", "status", "scope")): role = "категория, формат или область"
    elif any(word in low for word in ("node", "parent", "child")): role = "узел дерева или связь"
    elif any(word in low for word in ("result", "statistics", "stats", "data", "info")): role = "данные результата"
    elif any(word in low for word in ("cancel", "stop", "running")): role = "управление выполнением"
    elif any(word in low for word in ("progress", "callback", "handler", "observer")): role = "обратный вызов/уведомление"
    elif name == "argc": role = "число аргументов командной строки"
    elif name == "argv": role = "аргументы командной строки"
    elif any(word in low for word in ("index", "row", "column", "level")): role = "индекс или позиция"
    else: role = "входные данные операции"
    return f"{arg_type} {name} — {role}".strip()

def _describe_unit(name: str, kind: str, file_path: Path) -> str:
    known = {
        "AnalysisPipeline": "Координирует последовательное выполнение сканирования файловой системы и агрегации статистики.",
        "ApplicationController": "Организует выполнение анализа для приложения, передаёт прогресс/ошибки интерфейсу и обрабатывает запрос отмены.",
        "AnalysisResult": "Хранит результат анализа, корневой узел дерева каталогов и операции доступа к выбранному каталогу/статистике.",
        "DirectoryNode": "Представляет каталог в дереве результата, его путь, связи с дочерними каталогами и агрегированную статистику.",
        "DirectoryStatistics": "Хранит статистику каталога в разрезе непосредственных файлов и вложенных каталогов.",
        "FileInfo": "Временная запись о файловом объекте, используемая при чтении и классификации.",
        "CacheEntry": "Запись LRU-кэша: содержит список файлов каталога и позицию каталога в порядке недавнего использования.",
        "CategoryStatistics": "Содержит количество файлов и суммарный размер для одной категории файлов.",
        "CategoryValue": "Представляет категорию и числовые значения, подготовленные для построения диаграммы.",
        "DirectoryReadError": "Описывает ошибку чтения конкретного объекта/каталога, чтобы показать её и продолжить обработку доступных объектов.",
        "DirectoryReadResult": "Объединяет прочитанные элементы каталога и зарегистрированные ошибки чтения.",
        "ScanError": "Хранит сведения об ошибке, обнаруженной при сканировании файловой системы.",
        "ScanProgress": "Передаёт состояние текущего сканирования: объём выполненной работы и данные для обновления прогресса.",
        "ScopeStatistics": "Хранит агрегаты одной области статистики: число файлов, суммарный размер и статистику по категориям.",
        "DirectoryScanner": "Обходит файловую систему, строит дерево каталогов и собирает статистику непосредственно содержащихся файлов.",
        "DirectoryAggregator": "Вычисляет статистику вложенных каталогов по построенному дереву без повторного обращения к файловой системе.",
        "FileClassifier": "Определяет категорию файла по его расширению.",
        "DirectoryContentsReader": "Читает непосредственное содержимое выбранного каталога для отображения в интерфейсе.",
        "DirectoryCacheManager": "Кэширует локальные списки содержимого каталогов для ускорения навигации; не является владельцем дерева анализа.",
        "ExportManager": "Сериализует результат анализа в поддерживаемые форматы экспорта.",
        "VisualizationModel": "Преобразует статистику каталога в значения, необходимые для построения диаграммы.",
        "MainWindow": "Предоставляет основное окно приложения и связывает действия пользователя с отображением результата.",
        "IDirectoryScanner": "Интерфейс сканирования файловой системы.",
        "IDirectoryAggregator": "Интерфейс агрегации статистики дерева каталогов.",
        "IFileClassifier": "Интерфейс классификации файлов.",
    }
    if name in known:
        return known[name]
    if kind.startswith("enum"):
        return f"Задаёт конечный набор допустимых значений для параметра/состояния «{name}»."
    if kind == "struct":
        return f"Структура данных «{name}», объединяющая связанные поля предметной области; свойства и методы перечислены отдельно."
    if kind == "class":
        return f"Класс «{name}», инкапсулирующий данные и операции соответствующего компонента; свойства и методы перечислены отдельно."
    if kind in {"using", "typedef"}:
        return f"Объявляет псевдоним типа «{name}» для сокращения и унификации сигнатур."
    return f"Программная единица «{name}», объявленная в {relative(file_path)}."


def _extract_type_members(text: str, match: re.Match) -> list[str]:
    """Extract simple data-member declarations from a class/struct body."""
    opening = text.find("{", match.end())
    semicolon = text.find(";", match.end())
    if opening < 0 or (semicolon >= 0 and semicolon < opening):
        return []
    depth = 0
    closing = -1
    for index in range(opening, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                closing = index
                break
    if closing < 0:
        return []
    body = re.sub(r"/\*.*?\*/|//[^\n]*", "", text[opening + 1:closing], flags=re.S)
    members: list[str] = []
    for statement in body.split(";"):
        statement = re.sub(r"\b(public|private|protected)\s*:\s*", "", statement)
        statement = re.sub(r"\s+", " ", statement).strip()
        if not statement or "(" in statement or ")" in statement:
            continue
        if statement.startswith(("public:", "private:", "protected:", "friend ")):
            continue
        if re.search(r"\b[A-Za-z_]\w*\s*$", statement) and len(statement) < 220:
            members.append(statement + ";")
    return members


def _describe_function(name: str, declaration: str, file_path: Path) -> str:
    """Give a conservative purpose statement based on the operation's name and declaration."""
    known = {
        "addChild": "Создаёт дочерний узел каталога с указанным путём, добавляет связь в дерево и возвращает созданный узел.",
        "addFile": "Учитывает размер файла и увеличивает статистические счётчики соответствующей категории.",
        "aggregate": "Рассчитывает статистику вложенных каталогов по построенному дереву и возвращает результат выполнения агрегации.",
        "analysisFinished": "Qt-сигнал о завершении анализа; передаёт итоговый статус/результат подписчикам сигнала.",
        "analysisStarted": "Qt-сигнал о начале анализа, используемый для синхронизации интерфейса.",
        "appendDirectory": "Добавляет строку/запись каталога в представление списка содержимого.",
        "cachedDirectoryCount": "Возвращает количество каталогов, для которых в данный момент имеется запись в кэше.",
        "cachedFileCount": "Возвращает количество файлов, сохранённых в записях кэша.",
        "cancelAnalysis": "Устанавливает запрос отмены текущего анализа; фактическая остановка происходит в рабочем потоке при обработке запроса.",
        "categoryName": "Возвращает отображаемое название категории файлов для интерфейса/экспорта.",
        "chooseRootDirectory": "Открывает выбор корневого каталога и сохраняет выбранный пользователем путь, если выбор подтверждён.",
        "classify": "Определяет категорию файла на основании его имени/расширения.",
        "clear": "Очищает содержимое кэша и связанные структуры учёта недавно использованных записей.",
        "clearDirectoryCache": "Инвалидирует кэш содержимого каталогов, чтобы последующее чтение получило актуальные данные.",
        "evictLeastRecentlyUsed": "Удаляет из кэша запись каталога, которая дольше остальных не использовалась.",
        "exportJson": "Сериализует результат анализа в JSON и записывает его в выбранный файл.",
        "exportResult": "Выбирает реализацию экспорта согласно формату и передаёт ей результат анализа и путь выходного файла.",
        "exportTxt": "Формирует текстовое представление результата анализа и записывает его в файл.",
        "findChild": "Ищет дочерний узел с заданным путём среди непосредственных потомков текущего узла.",
        "findNode": "Находит узел дерева результата по пути каталога.",
        "findNodeRecursive": "Рекурсивно ищет узел с заданным путём в текущем поддереве.",
        "formatBytes": "Преобразует размер в байтах в удобную для отображения строку с единицей измерения.",
        "getCurrentNode": "Возвращает узел, выбранный в текущем результате анализа.",
        "getDirectCategoryCount": "Возвращает число файлов указанной категории, непосредственно находящихся в каталоге.",
        "getDirectCategorySize": "Возвращает суммарный размер файлов указанной категории непосредственно в каталоге.",
        "getRoot": "Возвращает корневой узел дерева каталогов результата анализа.",
        "getStatistics": "Возвращает сохранённую статистику соответствующего узла каталога.",
        "getSubtreeCategoryCount": "Возвращает количество файлов категории во вложенных каталогах, исключая непосредственные файлы текущего каталога.",
        "getSubtreeCategorySize": "Возвращает суммарный размер файлов категории во вложенных каталогах, исключая непосредственные файлы текущего каталога.",
        "getTotalCategoryCount": "Возвращает суммарное количество файлов категории в текущем каталоге и его поддереве.",
        "getTotalCategorySize": "Возвращает суммарный размер файлов категории в текущем каталоге и его поддереве.",
        "handleAnalysisFinished": "Обрабатывает сигнал завершения фоновой задачи и обновляет состояние/представление приложения.",
        "handleProgress": "Принимает сведения о ходе анализа и передаёт их элементам отображения прогресса.",
        "invalidate": "Удаляет или помечает недействительной кэшированную запись для указанного каталога.",
        "isEmpty": "Проверяет, содержит ли структура данных элементы.",
        "isRunning": "Возвращает признак того, выполняется ли анализ в данный момент.",
        "mergeSubtree": "Добавляет статистику вложенного каталога к статистике поддерева текущего каталога.",
        "navigateToChild": "Перемещает текущую позицию навигации к дочернему каталогу, если он существует.",
        "navigateToParent": "Перемещает текущую позицию навигации к родительскому каталогу, если он доступен.",
        "pathToQString": "Преобразует путь std::filesystem в строку QString для отображения в Qt-интерфейсе.",
        "populateTree": "Заполняет виджет дерева каталогов узлами из результата анализа.",
        "progressChanged": "Qt-сигнал об изменении прогресса; передаёт обновлённые данные подписчикам.",
        "read": "Читает непосредственное содержимое указанного каталога и формирует результат с элементами и ошибками чтения.",
        "refreshDirectoryContents": "Повторно читает содержимое выбранного каталога и обновляет его отображение без пересчёта полной статистики.",
        "scanError": "Qt-сигнал о возникшей при сканировании ошибке, передающий сведения обработчикам.",
        "setBusy": "Переключает состояние занятости интерфейса и доступность связанных действий.",
        "setCurrentDirectory": "Устанавливает текущий путь каталога для отображения в интерфейсе.",
        "setCurrentNode": "Устанавливает текущий узел дерева результата, используемый для отображения статистики.",
        "setRoot": "Устанавливает корневой узел результата анализа.",
        "showDirectoryContents": "Заполняет таблицу/список непосредственного содержимого выбранного каталога.",
        "showSelectedDirectory": "Обновляет представление выбранного каталога: содержимое, статистику и связанные элементы UI.",
        "showStatistics": "Обновляет таблицу статистики для выбранного каталога и области подсчёта.",
        "startAnalysis": "Проверяет возможность запуска, подготавливает состояние и запускает анализ в рабочем потоке.",
        "store": "Сохраняет список содержимого каталога в кэше и обновляет структуру LRU.",
        "touch": "Обновляет положение записи каталога в списке недавно использованных кэшированных элементов.",
        "updateChart": "Формирует и передаёт серии данных круговой диаграмме для текущей статистики.",
        "waitForFinished": "Ожидает завершения рабочего потока анализа, обеспечивая корректное завершение задачи.",
    }
    if name in known:
        return known[name]
    lower = name.lstrip("~").lower()
    if name.startswith("~"):
        return "Завершает время жизни экземпляра и освобождает принадлежащие ему ресурсы согласно реализации."
    if name == Path(relative(file_path)).stem:
        return "Создаёт экземпляр и устанавливает его начальное состояние."
    if lower.startswith(("get", "current", "root", "find", "lookup", "contains", "has", "is", "can")):
        return f"Получает или проверяет состояние/данные, относящиеся к операции «{name}»; точное условие и возвращаемое значение задаются телом реализации."
    if lower.startswith(("set", "select", "choose")):
        return f"Устанавливает или выбирает значение/объект для последующей работы компонента («{name}»)."
    if lower.startswith(("add", "append", "insert", "push", "register")):
        return f"Добавляет элемент, связь или обработчик в структуру данных компонента («{name}»)."
    if lower.startswith(("remove", "erase", "delete")):
        return f"Удаляет элемент или связь из структуры данных компонента («{name}»)."
    if lower.startswith(("clear", "reset", "invalidate")):
        return f"Очищает или сбрасывает состояние/кэш компонента («{name}»)."
    if any(word in lower for word in ("scan", "analy", "run", "execute", "process")):
        return f"Выполняет этап обработки или анализа данных («{name}»)."
    if any(word in lower for word in ("aggregate", "calculate", "compute", "build", "create")):
        return f"Формирует структуру данных или вычисляет производные значения («{name}»)."
    if any(word in lower for word in ("classify", "category")):
        return f"Определяет категорию или тип обрабатываемого объекта («{name}»)."
    if any(word in lower for word in ("read", "load", "contents", "refresh", "iterate", "open")):
        return f"Получает данные из источника или обновляет представление содержимого («{name}»)."
    if any(word in lower for word in ("export", "save", "write", "serialize")):
        return f"Подготавливает и/или сохраняет данные результата («{name}»)."
    if any(word in lower for word in ("cancel", "stop")):
        return f"Запрашивает или выполняет прекращение текущей операции («{name}»)."
    if any(word in lower for word in ("emit", "notify", "progress", "error", "finished", "on")):
        return f"Обрабатывает событие или передаёт уведомление между компонентами («{name}»)."
    return f"Выполняет операцию «{name}» в компоненте, объявленном в {relative(file_path)}; конкретная логика определяется телом метода."


def _find_function_body(name: str, files: list[Path], owner: str | None = None) -> str | None:
    """Return a matching C++ definition body when it can be identified conservatively."""
    if name.startswith("~"):
        searchable_name = re.escape(name)
    else:
        searchable_name = re.escape(name)
    for path in files:
        if path.suffix.lower() not in {".cpp", ".cc", ".cxx", ".c"}:
            continue
        text = read_text(path)
        owner_prefix = re.escape(owner) + r"::" if owner else r"(?:(?:[A-Za-z_]\w*)::)*"
        pattern = re.compile(
            r"(?m)^[ \t]*(?:[\w:<>,~*&]+\s+)*(?:(?:[A-Za-z_]\w*)::)*"
            + owner_prefix + searchable_name + r"\s*\("
        )
        for match in pattern.finditer(text):
            open_paren = text.find("(", match.start())
            depth = 0
            close_paren = -1
            for pos in range(open_paren, len(text)):
                if text[pos] == "(":
                    depth += 1
                elif text[pos] == ")":
                    depth -= 1
                    if depth == 0:
                        close_paren = pos
                        break
            if close_paren < 0:
                continue
            tail = text[close_paren + 1:close_paren + 500]
            brace_rel = tail.find("{")
            semi_rel = tail.find(";")
            if brace_rel < 0 or (semi_rel >= 0 and semi_rel < brace_rel):
                continue
            body_start = close_paren + 1 + brace_rel
            depth = 0
            for pos in range(body_start, len(text)):
                if text[pos] == "{":
                    depth += 1
                elif text[pos] == "}":
                    depth -= 1
                    if depth == 0:
                        return text[body_start:pos + 1]
    return None


def _describe_effects(name: str, declaration: str, body: str | None) -> str:
    """Describe effects from the declaration and available definition; state uncertainty explicitly."""
    if body is None:
        if "= 0" in declaration:
            return "Абстрактное объявление интерфейса: само по себе не выполняет операцию и не меняет данные; эффекты задаются конкретной реализацией."
        return "Тело определения не удалось сопоставить автоматически; по одной сигнатуре внешние изменения достоверно не определяются."
    effects: list[str] = []
    if re.search(r"\b(?:std::filesystem::directory_iterator|directory_iterator|file_size|status|symlink_status|ifstream|QIODevice::ReadOnly|QFileInfo|QDir::entryInfoList)\b", body):
        effects.append("читает сведения файловой системы или содержимое источника")
    if re.search(r"\b(?:std::ofstream|ofstream|QSaveFile|QFile|QIODevice::WriteOnly|QIODevice::ReadWrite|create_directories|remove_all)\b|\.write\s*\(", body):
        effects.append("может создавать, записывать или изменять внешний файл/ресурс")
    if re.search(r"\bemit\s+\w+|QMetaObject::invokeMethod|QCoreApplication::postEvent", body):
        effects.append("передаёт Qt-сигнал или уведомление другому компоненту")
    if re.search(r"\b(?:m_[A-Za-z_]\w*|[A-Za-z_]\w*_\s*(?:=|\+=|-=|\+\+|--)|this->\w*)", body) or re.search(r"\b[A-Za-z_]\w*_\.(?:push_back|emplace_back|clear|insert|erase|mergeSubtree|addFile|addChild|reset|invalidate)\s*\(", body):
        effects.append("изменяет состояние объекта или принадлежащую ему структуру данных")
    if not effects:
        if "const" in declaration:
            effects.append("в теле не выявлены очевидные записи во внешние ресурсы или поля объекта")
        else:
            effects.append("явные внешние записи не обнаружены; возможные изменения локального/внутреннего состояния следует проверять по полному телу")
    return "; ".join(effects) + "."


def _architecture_group(path: Path) -> str:
    """Map free functions to architectural elements rather than source files."""
    parts = [part.lower() for part in Path(relative(path)).parts]
    joined = "/".join(parts)
    if "tests" in parts: return "Тестирование"
    if "domain" in parts or any(token in joined for token in ("fileinfo", "categorystatistics", "directorynode", "analysisresult", "scope")): return "Доменная модель"
    if "infrastructure" in parts or any(token in joined for token in ("scanner", "classifier", "directorycontentsreader", "directorycachemanager", "exportmanager")): return "Инфраструктура"
    if "application" in parts or any(token in joined for token in ("analysispipeline", "applicationcontroller")): return "Прикладная логика"
    if "visualization" in parts or "visualizationmodel" in joined: return "Визуализация"
    if "ui" in parts or "mainwindow" in joined: return "Пользовательский интерфейс"
    if Path(relative(path)).name == "main.cpp": return "Точка входа и композиция приложения"
    return "Общие функции и вспомогательные компоненты"


def _describe_member(declaration: str, owner: str) -> str:
    """Describe a class field/property in one short sentence."""
    cleaned = re.sub(r"\s+", " ", declaration).strip().rstrip(";")
    match = re.search(r"([A-Za-z_]\w*)\s*(?:=.*)?$", cleaned)
    name = match.group(1) if match else cleaned
    low = name.lower()
    if any(token in low for token in ("path", "directory", "root")): role = "хранит путь или привязку к каталогу"
    elif any(token in low for token in ("size", "bytes", "count", "total")): role = "хранит размер или количество"
    elif any(token in low for token in ("category", "type", "status", "scope")): role = "хранит категорию, тип, область или состояние"
    elif any(token in low for token in ("child", "parent", "rootnode", "node")): role = "хранит связь или узел дерева"
    elif any(token in low for token in ("error", "message", "reason")): role = "хранит диагностические сведения"
    elif any(token in low for token in ("result", "statistic", "data", "info")): role = "хранит данные результата или статистики"
    elif any(token in low for token in ("cache", "entry", "lru", "iterator")): role = "поддерживает внутреннюю структуру компонента"
    else: role = f"хранит данные, необходимые классу {owner}"
    return role.capitalize() + "."


def _extract_class_spans(text: str) -> list[dict[str, object]]:
    """Find class/struct bodies so declarations can be attached to their owning type."""
    spans: list[dict[str, object]] = []
    for match in re.finditer(r"\b(class|struct)\s+([A-Za-z_]\w*)", text):
        opening = text.find("{", match.end())
        semi = text.find(";", match.end())
        if opening < 0 or (semi >= 0 and semi < opening): continue
        depth, closing = 0, -1
        for pos in range(opening, len(text)):
            if text[pos] == "{": depth += 1
            elif text[pos] == "}":
                depth -= 1
                if depth == 0:
                    closing = pos
                    break
        if closing >= 0:
            spans.append({"kind": match.group(1), "name": match.group(2), "start": match.start(), "body_start": opening + 1, "end": closing, "text": text[opening + 1:closing]})
    return spans


def extract_program_units(files: list[Path]) -> dict[str, list[list[object]]]:
    """Extract classes with their members, aliases/enums, and free functions."""
    groups: dict[str, list[list[object]]] = {
        "Интерфейсные классы": [], "Функциональные классы": [],
        "Перечисления и псевдонимы": [], "Функции и методы": [],
    }
    seen_types: set[tuple[str, str]] = set()
    seen_functions: set[tuple[str, str, str]] = set()
    class_records: dict[tuple[str, str], list[object]] = {}
    for path in files:
        if path.suffix.lower() not in {".h", ".hpp"}: continue
        text = read_text(path)
        spans = _extract_class_spans(text)
        for span in spans:
            kind, name = str(span["kind"]), str(span["name"])
            key = (kind, name)
            if key in seen_types: continue
            seen_types.add(key)
            normalized_path = relative(path).replace("\\", "/")
            group = "Интерфейсные классы" if name.startswith("I") or "/interfaces/" in normalized_path else "Функциональные классы"
            fields = _extract_type_members(text, re.search(r"\b" + kind + r"\s+" + re.escape(name) + r"\b", text))
            record: list[object] = [name, _describe_unit(name, kind, path), relative(path),
                                    [["Свойство", field, _describe_member(field, name)] for field in fields],
                                    [], kind]
            class_records[(relative(path), name)] = record
            groups[group].append(record)

        for match in re.finditer(r"\b(enum\s+class|enum|using|typedef)\s+([A-Za-z_]\w*)", text):
            kind, name = match.group(1), match.group(2)
            key = (kind, name)
            if key in seen_types: continue
            seen_types.add(key)
            if kind.startswith("enum"):
                body_start = text.find("{", match.end())
                body_end = text.find("}", body_start + 1) if body_start >= 0 else -1
                values = re.sub(r"\s+", " ", re.sub(r"/\*.*?\*/|//[^\n]*", "", text[body_start + 1:body_end], flags=re.S)).strip() if body_start >= 0 and body_end > body_start else "состав не извлечён автоматически"
                declaration = f"{kind} {name} {{ {values} }}"
            else:
                line_end = text.find(";", match.start())
                declaration = re.sub(r"\s+", " ", text[match.start():line_end + 1]).strip() if line_end >= 0 else f"{kind} {name}"
            groups["Перечисления и псевдонимы"].append([name, declaration, _describe_unit(name, kind, path), relative(path)])

        declaration_pattern = re.compile(r"(?m)^[ \t]*(?:virtual\s+|static\s+|inline\s+|explicit\s+|constexpr\s+|friend\s+|Q_INVOKABLE\s+|Q_SLOT\s+|Q_SIGNAL\s+)*(?:[\w:~<>]+(?:\s*[*&]+)?(?:\s+|\s*[*&]\s*)+)?[A-Za-z_~]\w*\s*\([^;{}]*\)\s*(?:const\s*)?(?:noexcept\s*)?(?:override\s*)?(?:final\s*)?(?:=\s*0\s*)?;")
        for match in declaration_pattern.finditer(text):
            declaration = re.sub(r"\s+", " ", match.group(0)).strip()
            name_match = re.search(r"([A-Za-z_~]\w*)\s*\((.*)\)", declaration)
            if not name_match: continue
            name, params_text = name_match.group(1), name_match.group(2)
            if name in {"if", "for", "while", "switch", "static_assert"}: continue
            owner_span = next((span for span in spans if int(span["body_start"]) <= match.start() < int(span["end"])), None)
            args = _split_parameters(params_text)
            arg_description = "\n".join(_argument_description(arg) for arg in args) if args else "Нет аргументов."
            if owner_span:
                owner_name = str(owner_span["name"])
                record = class_records.get((relative(path), owner_name))
                if record and isinstance(record[4], list):
                    return_match = re.match(r"(.*?)(?:\b" + re.escape(name) + r"\s*\()", declaration)
                    return_type = return_match.group(1).strip() if return_match else "тип не определён"
                    return_type = re.sub(r"^(?:(?:virtual|static|inline|explicit|constexpr|friend|Q_INVOKABLE|Q_SLOT|Q_SIGNAL)\s+)+", "", return_type).strip()
                    if name.startswith("~") or name == owner_name:
                        return_description = "Конструктор/деструктор — не имеет обычного возвращаемого значения."
                    elif not return_type or return_type == "void":
                        return_description = "void — значение не возвращается."
                    else:
                        return_description = f"{return_type} — возвращаемое значение метода."
                    method_body = _find_function_body(name, files, owner_name)
                    effect_description = _describe_effects(name, declaration, method_body)
                    args_description = "\n".join(_argument_description(arg) for arg in args) if args else "Нет аргументов."
                    record[4].append([name, declaration, args_description, _describe_function(name, declaration, path),
                                       return_description + " " + effect_description])
                continue
            key = (relative(path), name, declaration)
            if key in seen_functions: continue
            seen_functions.add(key)
            return_match = re.match(r"(.*?)(?:\b" + re.escape(name) + r"\s*\()", declaration)
            return_type = return_match.group(1).strip() if return_match else "тип не определён"
            return_type = re.sub(r"^(?:(?:virtual|static|inline|explicit|constexpr|friend|Q_INVOKABLE|Q_SLOT|Q_SIGNAL)\s+)+", "", return_type).strip()
            if name.startswith("~") or name == Path(relative(path)).stem: return_type = "конструктор/деструктор"
            result = "void — без возвращаемого значения" if not return_type or return_type == "void" else f"{return_type} — результат операции"
            body = _find_function_body(name, files, None)
            effects = _describe_effects(name, declaration, body)
            effect_summary = []
            if "изменяет состояние" in effects: effect_summary.append("изменяет состояние компонента")
            if "внешний файл" in effects: effect_summary.append("записывает внешний файл/ресурс")
            if "Qt-сигнал" in effects: effect_summary.append("передаёт Qt-сигнал/уведомление")
            if effect_summary: result += "; " + ", ".join(effect_summary)
            groups["Функции и методы"].append([name, declaration, args, arg_description, _describe_function(name, declaration, path), result, relative(path), _architecture_group(path)])

    main_path = next((path for path in files if relative(path) == "src/main.cpp"), None)
    if main_path:
        main_text = read_text(main_path)
        main_match = re.search(r"(?s)\bint\s+main\s*\((.*?)\)\s*\{", main_text)
        if main_match:
            params = _split_parameters(main_match.group(1))
            groups["Функции и методы"].append(["main", "int main(" + ", ".join(params) + ")", params, "Нет аргументов." if not params else "\n".join(_argument_description(arg) for arg in params), "Создаёт QApplication и основные зависимости, затем запускает цикл событий Qt.", "int — код завершения приложения; создаёт объекты приложения и UI.", relative(main_path), "Точка входа и композиция приложения"])
    for group in groups:
        groups[group].sort(key=lambda row: (str(row[0]).lower(), str(row[2] if len(row) > 2 else "")))
    return groups

def insert_static_table_of_contents(doc: Document) -> None:
    """Insert a visible table of contents based on actual report headings."""
    headings: list[tuple[str, int]] = []
    for paragraph in doc.paragraphs:
        style_name = paragraph.style.name if paragraph.style is not None else ""
        if style_name not in {"Heading 1", "Heading 2", "Heading 3"}:
            continue
        title = paragraph.text.strip()
        if not title or re.match(r"^\d+\. (src/|tests/|CMakeLists|\.github/)", title):
            continue
        level = int(style_name.split()[-1])
        headings.append((title, level))

    assignment = next((p._p for p in doc.paragraphs if p.text.strip() == "Задание"), None)
    if assignment is None:
        raise RuntimeError("Cannot insert table of contents: section 'Задание' not found")

    elements = []
    title = doc.add_paragraph()
    title.style = "Title"
    title.alignment = WD_ALIGN_PARAGRAPH.CENTER
    title.add_run("Оглавление")
    elements.append(title._p)

    for text, level in headings:
        style_name = f"TOC Entry {level}"
        if style_name not in doc.styles:
            toc_style = doc.styles.add_style(style_name, WD_STYLE_TYPE.PARAGRAPH)
            toc_style.base_style = doc.styles["Normal"]
        paragraph = doc.add_paragraph(style=style_name)
        paragraph.paragraph_format.space_after = Pt(2)
        paragraph.paragraph_format.left_indent = Cm(0.6 * (level - 1))
        paragraph.paragraph_format.keep_with_next = False
        paragraph.paragraph_format.tab_stops.add_tab_stop(Cm(15.5), WD_TAB_ALIGNMENT.RIGHT, WD_TAB_LEADER.DOTS)
        run = paragraph.add_run(text)
        run.font.name = "Times New Roman"
        run.font.size = Pt(10 if level > 1 else 11)
        if level == 1:
            run.bold = True
        paragraph.add_run("\t")
        elements.append(paragraph._p)

    page_break = doc.add_paragraph()
    page_break.add_run().add_break()
    page_break.paragraph_format.page_break_before = True
    elements.append(page_break._p)

    # Move the newly created paragraphs from the document end to the front matter.
    for element in elements:
        assignment.addprevious(element)



def add_title_page(doc: Document) -> None:
    """Reproduce the provided department example's title-page order and spacing."""
    lines = [
        ("МИНИСТЕРСТВО НАУКИ И ВЫСШЕГО ОБРАЗОВАНИЯ РОССИЙСКОЙ ФЕДЕРАЦИИ ФЕДЕРАЛЬНОЕ ГОСУДАРСТВЕННОЕ АВТОНОМНОЕ ОБРАЗОВАТЕЛЬНОЕ УЧРЕЖДЕНИЕ ВЫСШЕГО ОБРАЗОВАНИЯ", WD_ALIGN_PARAGRAPH.CENTER, False),
        ("МОСКОВСКИЙ АВИАЦИОННЫЙ ИНСТИТУТ", WD_ALIGN_PARAGRAPH.CENTER, True),
        ("(национальный исследовательский университет)", WD_ALIGN_PARAGRAPH.CENTER, True),
        ("Институт № 7 «Интеллектуальные и робототехнические системы»", WD_ALIGN_PARAGRAPH.CENTER, False),
        ("Кафедра 703", WD_ALIGN_PARAGRAPH.CENTER, False),
        ("Курсовая работа по курсу", WD_ALIGN_PARAGRAPH.CENTER, True),
        ("«Технологии разработки программного обеспечения»", WD_ALIGN_PARAGRAPH.CENTER, True),
        ("", WD_ALIGN_PARAGRAPH.CENTER, False),
        ("«Разработка оконного приложения для анализа дискового пространства файловой системы»", WD_ALIGN_PARAGRAPH.CENTER, True),
        ("", WD_ALIGN_PARAGRAPH.RIGHT, False),
        ("", WD_ALIGN_PARAGRAPH.RIGHT, False),
        ("", WD_ALIGN_PARAGRAPH.RIGHT, False),
        ("Выполнил студент группы М70-407С-22", WD_ALIGN_PARAGRAPH.RIGHT, False),
        ("Осипенко Гавриил Васильевич____________", WD_ALIGN_PARAGRAPH.RIGHT, False),
        ("Принял старший преподаватель кафедры 703", WD_ALIGN_PARAGRAPH.RIGHT, False),
        ("Барчев Николай Борисович____________", WD_ALIGN_PARAGRAPH.RIGHT, False),
        ("Дата сдачи отчета", WD_ALIGN_PARAGRAPH.RIGHT, False),
        ("_______________", WD_ALIGN_PARAGRAPH.RIGHT, False),
        ("", None, False),
        ("", None, False),
        ("", None, False),
        ("", WD_ALIGN_PARAGRAPH.CENTER, False),
        ("", WD_ALIGN_PARAGRAPH.CENTER, False),
        ("", WD_ALIGN_PARAGRAPH.CENTER, False),
        ("Москва", WD_ALIGN_PARAGRAPH.CENTER, False),
        ("2026", WD_ALIGN_PARAGRAPH.CENTER, False),
    ]
    for text, alignment, bold in lines:
        p = doc.add_paragraph()
        if alignment is not None:
            p.alignment = alignment
        run = p.add_run(text)
        run.font.name = "Times New Roman"
        run.font.size = Pt(14)
        run.bold = bold
    doc.add_page_break()


def add_pseudocode(doc: Document) -> None:
    doc.add_heading("Псевдокод алгоритмов решения задачи", level=1)
    algorithms = [
        ("Основной алгоритм работы программы", """НАЧАЛО
  Получить путь каталога от пользователя
  Проверить возможность начать анализ
  Запустить анализ вне потока пользовательского интерфейса
  Построить дерево каталогов и собрать Direct-статистику
  Если анализ завершён или отменён:
    вычислить статистику вложенных каталогов снизу вверх
    передать результат интерфейсу вместе со статусом и ошибками
  Отобразить дерево, статистику и данные диаграммы
  При выборе каталога обновить представление по сохранённому результату
  При экспорте сериализовать агрегированные данные в выбранный формат
КОНЕЦ"""),
        ("Представление дерева каталогов", """Для каждого доступного каталога создать узел
  Сохранить путь каталога и связь с родительским/дочерними узлами
  Записать статистику файлов, непосредственно находящихся в каталоге
Не хранить постоянный глобальный список всех файлов как основу навигации
После сканирования сохранить дерево и агрегированную статистику"""),
        ("Сканирование файловой системы", """Поместить корневой каталог в очередь/стек обхода
Пока имеются каталоги для обработки и анализ не отменён:
  получить очередной каталог
  перечислить его непосредственное содержимое
  для каждого объекта:
    обработать ошибку доступа, если она возникла
    если объект — обычный файл:
      получить размер
      определить категорию
      увеличить Direct-счётчики текущего каталога
    иначе если объект — каталог и его не следует пропускать:
      создать дочерний узел и добавить его в очередь
    иначе:
      не обходить цель символической ссылки на каталог рекурсивно
Зафиксировать итоговый статус, прогресс и ошибки"""),
        ("Классификация файла", """Получить расширение имени файла
Нормализовать расширение способом, используемым реализацией
Найти расширение в таблице соответствий категориям
Если соответствие найдено — вернуть категорию
Иначе вернуть Unknown или Other согласно контракту классификатора"""),
        ("Агрегация статистики", """Обработать дерево снизу вверх
Для каждого узла:
  начать Subtree-статистику с нулевых значений
  для каждого дочернего каталога:
    добавить его Direct- и Subtree-статистику в Subtree текущего узла
  вычислить Total как Direct + Subtree
Не обращаться к файловой системе на этапе агрегации"""),
        ("Подготовка диаграммы", """Получить статистику текущего каталога
Определить выбранную область: Direct, Subtree или Total
Для каждой категории взять размер выбранной области
Вычислить долю от общего размера; при нулевом размере долю считать нулевой
Передать подписи, размеры и доли модели визуализации"""),
        ("Навигация по каталогу", """Получить выбранный узел/путь из дерева интерфейса
Найти соответствующий узел в результате анализа
Обновить статистику и диаграмму для выбранного узла
Получить список непосредственного содержимого через reader/cache
Не запускать полное сканирование повторно только ради навигации"""),
        ("Экспорт результата", """Получить текущий результат анализа и выбранный формат
Проверить доступность выходного файла
Сериализовать сохранённые агрегированные данные в JSON или TXT
Записать файл и обработать возможную ошибку записи
Сообщить интерфейсу об успешном сохранении или ошибке"""),
    ]
    for title, code in algorithms:
        doc.add_heading(title, level=2)
        add_code_block(doc, code)


def add_implementation(doc: Document, source: list[Path]) -> None:
    """Describe types and functions in compact tables grouped by architecture."""
    doc.add_heading("Сведения о программной реализации", level=1)
    doc.add_heading("Сведения о выбранном языке программирования", level=2)
    add_paragraph(doc, "Программа реализована на C++17 (стандарт языка C++ 2017 года). Для интерфейса и визуализации используются Qt 5 — набор библиотек и инструментов разработки; модули Core (базовые типы и сервисы), Widgets (элементы графического интерфейса) и Charts (диаграммы). CMake — система конфигурирования сборки; CTest — средство запуска тестов; GitHub Actions — система автоматизации сборки и проверок в CI (непрерывной интеграции).")
    doc.add_heading("Описание входных и выходных данных", level=2)
    add_table(doc, ["Вид", "Данные", "Назначение"], [
        ["Вход", "Путь к корневому каталогу", "Определяет область анализа."],
        ["Вход", "Состояние файловой системы и права доступа", "Определяют доступность объектов и полноту результата."],
        ["Вход", "Команды интерфейса", "Запуск/отмена анализа, выбор каталога, переключение статистики и экспорт."],
        ["Внутренние данные", "Путь, тип, размер и категория объекта", "Используются сканером, классификатором и агрегатором."],
        ["Выход", "Дерево каталогов и агрегированная статистика", "Навигация и просмотр результатов без повторного полного обхода."],
        ["Выход", "Таблицы, диаграмма, прогресс и ошибки", "Представление результата пользователю."],
        ["Выход", "JSON или TXT", "Сохранение агрегированного результата."],
    ], [3.0, 6.0, 8.0])

    landscape = doc.add_section(WD_SECTION.NEW_PAGE)
    landscape.orientation = WD_ORIENT.LANDSCAPE
    landscape.page_width, landscape.page_height = landscape.page_height, landscape.page_width
    landscape.top_margin = Cm(1.4); landscape.bottom_margin = Cm(1.4)
    landscape.left_margin = Cm(1.3); landscape.right_margin = Cm(1.3)

    doc.add_heading("Описание спроектированных типов и программных единиц", level=2)
    add_paragraph(doc, "В таблицах различаются классы и структуры данных согласно объявлениям в исходном коде. Для каждого типа отдельно приведены таблицы свойств и методов: у свойств указаны объявление и назначение, у методов — сигнатура, аргументы, назначение и возвращаемое значение/эффекты. Методы классов не дублируются в общей таблице свободных функций. Свободные функции сгруппированы по архитектурным элементам.")
    doc.add_heading("Расшифровка сокращений", level=3)
    add_table(doc, ["Сокращение", "Расшифровка", "Значение в проекте"], [
        ["API", "Application Programming Interface — программный интерфейс приложения", "Набор доступных операций и контрактов взаимодействия компонентов."],
        ["CI", "Continuous Integration — непрерывная интеграция", "Автоматическая сборка и проверка изменений."],
        ["CMake", "Система конфигурирования сборки", "Подготовка сборочных файлов для поддерживаемых платформ."],
        ["CTest", "Средство запуска тестов CMake", "Запуск зарегистрированных автоматических тестов."],
        ["JSON", "JavaScript Object Notation — текстовый формат обмена данными", "Формат экспорта результатов анализа."],
        ["LRU", "Least Recently Used — давно не использовавшийся элемент", "Принцип вытеснения старейшей по использованию записи из кэша."],
        ["Qt", "Кроссплатформенный набор библиотек разработки", "Создание графического интерфейса, сигналов/слотов и диаграмм."],
        ["TXT", "Text — текстовый формат файла", "Формат экспорта результатов в обычный текст."],
        ["UI", "User Interface — пользовательский интерфейс", "Элементы взаимодействия пользователя с приложением."],
        ["UML", "Unified Modeling Language — унифицированный язык моделирования", "Язык визуального моделирования программных систем."],
    ], [2.5, 8.0, 12.0])
    units = extract_program_units(source)
    for group_name in ("Интерфейсные классы", "Функциональные классы"):
        doc.add_heading(group_name, level=3)
        rows = units[group_name]
        if not rows:
            add_paragraph(doc, "В текущем исходном коде программные единицы этой группы не обнаружены.")
            continue
        add_table(doc, ["Тип", "Категория", "Назначение", "Файл"],
                  [[str(r[0]), "Класс" if str(r[5]) == "class" else "Структура (struct)", str(r[1]), str(r[2])] for r in rows],
                  [3.2, 3.2, 13.0, 4.0])
        for row in rows:
            type_name, description, file_name = str(row[0]), str(row[1]), str(row[2])
            properties, methods, kind = row[3], row[4], str(row[5])
            type_label = "Класс" if kind == "class" else "Структура"
            doc.add_heading(f"{type_label} {type_name}", level=4)
            add_paragraph(doc, description)
            doc.add_heading("Свойства", level=5)
            if isinstance(properties, list) and properties:
                add_table(doc, ["Объявление / тип", "Назначение"],
                          [[str(m[1]), str(m[2])] for m in properties], [10.0, 13.0])
            else:
                add_paragraph(doc, "Свойства не обнаружены в объявлении типа.")
            doc.add_heading("Методы", level=5)
            if isinstance(methods, list) and methods:
                add_table(doc, ["Метод и сигнатура", "Аргументы: тип, имя и роль", "Назначение", "Возвращаемое значение / эффекты"],
                          [[str(m[1]), str(m[2]), str(m[3]), str(m[4])] for m in methods],
                          [6.0, 6.0, 6.5, 6.5])
            else:
                add_paragraph(doc, "Методы не обнаружены автоматически в объявлении типа.")
    doc.add_heading("Перечисления и псевдонимы", level=3)
    aliases = units["Перечисления и псевдонимы"]
    if aliases:
        add_table(doc, ["Имя", "Объявление", "Назначение", "Файл"], [[str(r[0]), str(r[1]), str(r[2]), str(r[3])] for r in aliases], [3.0, 8.0, 10.0, 4.0])
    else:
        add_paragraph(doc, "Перечисления и псевдонимы не обнаружены.")
    doc.add_heading("Свободные функции", level=3)
    function_rows = units["Функции и методы"]
    if function_rows:
        by_architecture: dict[str, list[list[object]]] = {}
        for row in function_rows: by_architecture.setdefault(str(row[7]), []).append(row)
        for architecture, rows in sorted(by_architecture.items()):
            doc.add_heading(architecture, level=4)
            table_rows = [[str(r[0]), str(r[1]), str(r[3] or "Нет аргументов."), str(r[4]), str(r[5]), str(r[6])] for r in rows]
            add_table(doc, ["Функция", "Сигнатура", "Аргументы: тип, имя и роль", "Назначение", "Возвращаемое значение / изменения", "Файл"], table_rows, [2.7, 4.5, 5.0, 5.0, 4.5, 3.0])
    else:
        add_paragraph(doc, "Свободные функции в заголовочных файлах текущего исходного кода не обнаружены.")

    portrait = doc.add_section(WD_SECTION.NEW_PAGE)
    portrait.orientation = WD_ORIENT.PORTRAIT
    portrait.page_width, portrait.page_height = portrait.page_height, portrait.page_width
    portrait.top_margin = Cm(2); portrait.bottom_margin = Cm(2)
    portrait.left_margin = Cm(2.5); portrait.right_margin = Cm(1.5)
    doc.add_heading("Архитектура и потоки данных", level=2)
    add_architecture_diagram(doc)
    doc.add_heading("Инструкция пользователя", level=2)
    add_numbered(doc, [
        "Запустить переносимую сборку для Windows x64 или Linux x86_64 либо собрать проект из исходного кода.",
        "Указать путь к каталогу или выбрать каталог в диалоге.",
        "Запустить анализ и дождаться завершения; при необходимости отменить задачу.",
        "Выбирать каталоги в дереве и просматривать статистику.",
        "Переключать область диаграммы: Direct («Только здесь»), Subtree («Вложенные каталоги») или Total («Весь каталог»).",
        "При необходимости экспортировать результат в JSON или TXT.",
        "После изменения файловой системы повторить полный анализ: обновление непосредственного содержимого не пересчитывает всё дерево.",
    ])
    doc.add_heading("Состав исходных файлов", level=2)
    add_paragraph(doc, f"В листинг включено файлов: {len(source)}. Полный перечень приведён в разделе «Листинг».")
    for p in source: add_paragraph(doc, relative(p))

def add_testing(doc: Document, source: list[Path]) -> None:
    doc.add_heading("Тестирование", level=1)
    test_files = sorted((ROOT / "tests").glob("*.cpp")) if (ROOT / "tests").exists() else []
    doc.add_heading("Автоматические тесты", level=2)
    if test_files:
        rows = [[p.stem, relative(p), "Запуск и результат CTest для текущего commit приведены ниже."] for p in test_files]
        add_table(doc, ["Тестовый модуль", "Файл", "Результат"], rows)
    else:
        add_paragraph(doc, "В каталоге tests не обнаружены файлы автоматических тестов.")
    ci_status = os.environ.get("REPORT_CTEST_STATUS", "").strip()
    ci_sha = os.environ.get("REPORT_COMMIT_SHA", "").strip() or os.environ.get("GITHUB_SHA", "").strip()
    ci_url = os.environ.get("REPORT_CI_RUN_URL", "").strip()
    if ci_status.lower() in {"passed", "success", "successfully"}:
        add_paragraph(doc, f"Фактический результат CI для commit {ci_sha or 'не указан'}: сборка и CTest завершились успешно в GitHub Actions. Запуск: {ci_url or 'URL текущего запуска не передан'}.")
    elif ci_status:
        add_paragraph(doc, f"Статус автоматической проверки, переданный средой CI: {ci_status}. Commit: {ci_sha or 'не указан'}. Запуск: {ci_url or 'URL не передан'}.")
    else:
        add_paragraph(doc, "Фактический статус CI не передан генератору. Генерация без подтверждённого статуса не должна использоваться как финальная версия для сдачи.")

    doc.add_heading("Ручное тестирование", level=2)
    add_paragraph(doc, "Ручные испытания выполнены на Windows 10 Pro 22H2, архитектура x64, на переносимой сборке приложения. Проверялись запуск приложения, корректность анализа контрольного набора, отображение статистики, навигация, экспорт, обработка ошибок, отзывчивость интерфейса и устойчивость при длительном анализе.")
    scenarios = [
        ["Запуск переносимой сборки и повторный запуск", "Главное окно открывается, приложение работает без аварийного завершения."],
        ["Контрольный набор TestData: пять файлов, суммарно 1500 байт", "Общее количество файлов, суммарный размер и размеры каталогов совпадают с контрольными значениями."],
        ["Пустой каталог, вложенные каталоги, неизвестные расширения и файлы разного размера", "Анализ завершается; файлы учитываются один раз; Direct, Subtree и Total соответствуют структуре каталога."],
        ["Статистика по категориям и круговая диаграмма", "Размеры и процентные доли согласуются с агрегированной статистикой; диаграмма обновляется при выборе каталога."],
        ["Переходы между каталогами после завершения анализа", "Навигация использует сохранённый результат без повторного полного сканирования."],
        ["Экспорт результатов в JSON и TXT", "Файлы создаются, содержат статистику анализа и согласуются с данными интерфейса."],
        ["Символическая ссылка на каталог, в том числе рекурсивная", "Целевой каталог не обходится повторно; зацикливания при сканировании не возникает."],
        ["Отмена выбора каталога, отмена анализа, изменение файлов во время сканирования", "Приложение остаётся работоспособным; состояние операции и доступные результаты отображаются штатно."],
        ["Отзывчивость интерфейса, прогресс, закрытие приложения и повторные анализы", "Окно остаётся отзывчивым; операции завершаются без аварий; повторные запуски сохраняют работоспособность."],
        ["Большое количество файлов и глубокая вложенность каталогов", "Сканирование завершается, структура и статистика остаются согласованными."],
        ["Доступ к каталогам с ограниченными правами", "Ошибки доступа обрабатываются без аварийного завершения; недоступные объекты могут быть исключены из подсчёта и отражены в журнале ошибок."],
    ]
    add_table(doc, ["Проведённая проверка", "Результат"], scenarios)
    add_paragraph(doc, "Итог ручного тестирования: все проведённые проверки актуальной версии пройдены, замечаний по результатам повторной проверки нет. Доступ к отдельным системным каталогам зависит от прав операционной системы; их содержимое нельзя считать измеренным, если система отказала в доступе.")


def _annotate_source_for_report(path: Path, source_text: str) -> str:
    """Add explanatory comments to the report copy; repository files remain unchanged."""
    rel = relative(path)
    suffix = path.suffix.lower()
    if suffix in {".h", ".hpp", ".cpp", ".cc", ".cxx", ".c"}:
        output: list[str] = []
        for line in source_text.splitlines():
            stripped = line.strip()
            class_match = re.match(r"(?:class|struct)\s+(\w+)", stripped)
            if class_match:
                output.append(f"// {class_match.group(1)}: объявление типа и его контракт.")
            elif re.match(r"(?:enum\s+class|enum)\s+\w+", stripped):
                output.append("// Перечисление задаёт допустимые именованные значения.")
            elif re.match(r"^[\w:<>,~*& ]+\b[A-Za-z_~]\w*\s*\([^;]*\)\s*(?:const\s*)?(?:override\s*)?(?:\{|;)?\s*$", stripped) and not stripped.startswith(("if ", "for ", "while ", "switch ", "return ")):
                nm = re.search(r"([A-Za-z_~]\w*)\s*\(", stripped)
                if nm and nm.group(1) not in {"if", "for", "while", "switch", "catch"}:
                    output.append(f"// {nm.group(1)}: объявление/реализация операции; детали поясняются ниже.")
            if re.match(r"(if|else if|else|for|while|switch|case|try|catch)\b", stripped):
                output.append(f"// Управление потоком: {stripped.rstrip('{').strip()}.")
            elif stripped.startswith("emit "):
                output.append("// Уведомить подписчиков Qt-сигнала о событии.")
            elif stripped.startswith(("return ", "return;")):
                output.append("// Вернуть результат или завершить текущую операцию.")
            elif re.search(r"\b(open|write|flush|close|file_size|directory_iterator|create_directories)\s*\(", stripped):
                output.append("// Обращение к файловой системе или операция ввода-вывода.")
            output.append(line)
        return "\n".join(output) + ("\n" if source_text.endswith("\n") else "")
    if rel == "CMakeLists.txt":
        output = []
        for line in source_text.splitlines():
            if line.strip().startswith(("project(", "set(", "find_package(", "add_executable(", "add_library(", "target_link_libraries(", "add_subdirectory(", "enable_testing(", "add_test(")):
                output.append("# Настройка проекта, зависимости, цели сборки или тесты.")
            output.append(line)
        return "\n".join(output) + "\n"
    if suffix in {".yml", ".yaml"}:
        output = []
        for line in source_text.splitlines():
            if line.strip().endswith(":") and not line.strip().startswith("#"):
                output.append("# Этап CI/CD: " + line.strip().rstrip(":"))
            output.append(line)
        return "\n".join(output) + "\n"
    return source_text


def add_listing(doc: Document, source: list[Path]) -> None:
    doc.add_heading("Листинг", level=1)
    add_paragraph(doc, "Приведены все исходные файлы текущего checkout. В копии листинга добавлены пояснения к объявлениям, ветвлениям, циклам и ключевым операциям; файлы репозитория не изменяются.")
    for index, path in enumerate(source, start=1):
        doc.add_heading(f"{index}. {relative(path)}", level=2)
        original = read_text(path)
        annotated = _annotate_source_for_report(path, original)
        if not annotated.strip(): add_paragraph(doc, "[Файл пуст]")
        else: add_code_block(doc, annotated, as_table=True)
        doc.add_paragraph()

def add_architecture_diagram(doc: Document) -> None:
    """Create an editable Word-native flow diagram; SmartArt authoring is not exposed by python-docx."""
    doc.add_heading("Схема архитектуры и потоков данных", level=2)
    add_paragraph(doc, "Основной поток анализа идёт сверху вниз; нижний уровень разветвляет результат на пользовательский интерфейс, визуализацию и экспорт.")
    rows = [
        ["Пользователь / MainWindow (UI)", "Команды: выбор каталога, запуск/отмена анализа, навигация, экспорт"],
        ["↓", "↓"],
        ["ApplicationController", "Управляет рабочей задачей, прогрессом, отменой и передачей результата"],
        ["↓", "↓"],
        ["AnalysisPipeline", "Последовательно запускает сканирование и агрегацию"],
        ["↓", "↓"],
        ["DirectoryScanner + FileClassifier", "Обход файловой системы, классификация, дерево каталогов и Direct-статистика"],
        ["↓", "↓"],
        ["DirectoryAggregator", "Вычисляет Subtree и Total снизу вверх по дереву в памяти"],
        ["↓", "↓"],
        ["AnalysisResult", "Хранит дерево и агрегированную статистику"],
        ["↙", "↓                                      ↘"],
        ["UI: таблицы и навигация", "VisualizationModel → Qt Charts          ExportManager → JSON/TXT"],
    ]
    table = add_table(doc, ["Элемент потока", "Действие / выходные данные"], rows, [7.0, 15.0])
    for index, row in enumerate(table.rows[1:], start=1):
        if index in {1, 3, 5, 7, 9, 11}:
            for cell in row.cells:
                for para in cell.paragraphs:
                    para.alignment = WD_ALIGN_PARAGRAPH.CENTER
                    for run in para.runs:
                        run.bold = True
                        run.font.size = Pt(10)
    add_paragraph(doc, "Дополнительный путь навигации: DirectoryContentsReader читает непосредственное содержимое выбранного каталога, а DirectoryCacheManager кэширует список для интерфейса. Этот путь не пересчитывает агрегированную статистику всего дерева.")
    add_paragraph(doc, "Примечание реализации: python-docx не предоставляет API создания нативного объекта SmartArt. Поэтому генератор формирует редактируемую схему из стандартных элементов таблицы Word; для преобразования именно в SmartArt потребуется отдельная операция в Microsoft Word.")

def add_contracts_appendix(doc: Document) -> None:
    doc.add_heading("Приложение. Контракты программных компонентов", level=1)
    add_paragraph(doc, "Контракты фиксируют ответственность компонентов, входные данные, результаты и ограничения взаимодействия. Точные сигнатуры типов и операций приведены в таблицах программных единиц и листинге.")
    rows = [
        ["FileInfo", "Временная запись о файловом объекте: путь, размер, категория и тип.", "Используется при сканировании и классификации; не является постоянным хранилищем полного списка файлов."],
        ["DirectoryStatistics / ScopeStatistics / CategoryStatistics", "Хранят число файлов, суммарный размер и распределение по категориям для областей Direct, Subtree и Total.", "Total согласуется с суммой Direct и Subtree; категории и агрегаты должны быть согласованы между собой."],
        ["DirectoryNode", "Представляет каталог в дереве результата, хранит путь, статистику и связи с дочерними каталогами.", "Не выполняет файловый обход, глобальную навигацию или хранение отдельной записи на каждый файл."],
        ["AnalysisResult", "Содержит результат анализа и предоставляет доступ к корневому и выбранному узлу и их статистике.", "Навигация по готовому результату не запускает полный повторный обход."],
        ["DirectoryScanner", "Обходит доступную часть файловой системы, создаёт узлы дерева, классифицирует файлы и заполняет Direct-статистику.", "Не рассчитывает Subtree-статистику; ошибки доступа фиксируются согласно реализации."],
        ["DirectoryAggregator", "Рассчитывает Subtree и Total по дереву снизу вверх.", "Работает с данными в памяти и не обращается повторно к файловой системе."],
        ["FileClassifier", "Определяет категорию файла по расширению.", "Не анализирует MIME-тип или сигнатуру содержимого; неизвестные расширения обрабатываются по правилам классификатора."],
        ["AnalysisPipeline", "Координирует этапы сканирования и агрегации.", "Передаёт контроллеру результат и состояние выполнения."],
        ["ApplicationController", "Запускает и отменяет анализ, сообщает о прогрессе, ошибках и завершении.", "Долгий анализ выполняется вне GUI-потока."],
        ["DirectoryContentsReader / DirectoryCacheManager", "Читают и кэшируют непосредственное содержимое выбранных каталогов.", "Кэш относится к отображению и не является владельцем дерева результата."],
        ["VisualizationModel", "Преобразует статистику выбранной области в значения категорий для диаграммы.", "Доли рассчитываются относительно суммарного размера выбранной области; нулевая сумма не должна приводить к делению на ноль."],
        ["ExportManager", "Сериализует сохранённый результат в JSON или TXT.", "Экспорт не должен запускать новое сканирование; ошибки записи возвращаются интерфейсу."],
    ]
    add_table(doc, ["Компонент / структура", "Входные данные и ответственность", "Выходные данные и ограничения"], rows)


def add_conclusion(doc: Document) -> None:
    doc.add_heading("Заключение", level=1)
    add_paragraph(doc, "В курсовом проекте реализуется оконное приложение на C++17 и Qt 5 для анализа использования дискового пространства. Реализация включает собственные компоненты обхода файловой системы, классификации, построения дерева каталогов, агрегации статистики, визуализации и экспорта. Фактический состав функций и результаты проверки должны оцениваться по исходному коду и тестам выбранного commit.")
    doc.add_heading("Известные ограничения", level=2)
    add_bullets(doc, [
        "Классификация файлов выполняется по расширению; MIME-тип и сигнатура содержимого не анализируются.",
        "Символические ссылки на каталоги не обходятся рекурсивно.",
        "Недоступные объекты могут приводить к неполной статистике.",
        "Обновление списка непосредственного содержимого не заменяет повторный полный анализ.",
        "Сборка и smoke-test в CI не доказывают корректность каждого пользовательского сценария на каждой целевой системе.",
    ])


def validate_docx(path: Path, expected_sections: list[str], expected_listing_count: int) -> None:
    if not path.exists() or path.stat().st_size < 10_000:
        raise RuntimeError(f"DOCX missing or suspiciously small: {path}")
    if not zipfile.is_zipfile(path):
        raise RuntimeError(f"Generated file is not a valid ZIP/DOCX container: {path}")
    doc = Document(path)
    text = "\n".join(p.text for p in doc.paragraphs)
    for table in doc.tables:
        for row in table.rows:
            text += "\n" + "\n".join(cell.text for cell in row.cells)
    missing = [section for section in expected_sections if section not in text]
    if missing:
        raise RuntimeError(f"Required sections missing from DOCX: {missing}")
    listing_headings = [p.text for p in doc.paragraphs if re.match(r"^\d+\. (src/|tests/|CMakeLists|\.github/)", p.text)]
    if len(listing_headings) < expected_listing_count:
        raise RuntimeError(f"Listing inventory mismatch: expected at least {expected_listing_count}, found {len(listing_headings)}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT, help="Output DOCX path")
    args = parser.parse_args()
    output = args.output if args.output.is_absolute() else (ROOT / args.output).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)

    if not ASSIGNMENT.exists() or not REPORT_SPEC.exists():
        raise SystemExit("Required reporting source documents are missing. See docs/reporting/.")

    sources = source_files()
    if not sources:
        raise SystemExit("No source files were found. Run from a complete repository checkout.")

    doc = Document()
    configure_document(doc)
    for section in doc.sections:
        add_page_number(section)
    add_title_page(doc)

    doc.add_heading("Задание", level=1)
    add_paragraph(doc, markdown_quote_text(ASSIGNMENT))
    add_pseudocode(doc)
    add_implementation(doc, sources)
    add_listing(doc, sources)
    add_testing(doc, sources)
    add_conclusion(doc)
    add_contracts_appendix(doc)
    insert_static_table_of_contents(doc)

    doc.core_properties.title = "Отчёт по курсовой работе: Disk Analyzer"
    doc.core_properties.subject = "ТРПО, C++17, Qt 5"
    doc.core_properties.author = "Осипенко Гавриил Васильевич"
    doc.save(output)

    expected = ["Оглавление", "Задание", "Псевдокод алгоритмов решения задачи", "Сведения о программной реализации", "Расшифровка сокращений", "Интерфейсные классы", "Функциональные классы", "Перечисления и псевдонимы", "Свободные функции", "Листинг", "Тестирование", "Заключение", "Схема архитектуры и потоков данных", "Приложение. Контракты программных компонентов"]
    validate_docx(output, expected, len(sources))
    manifest = output.with_suffix(".manifest.txt")
    manifest.write_text(
        "Generated report inventory\n"
        f"Output: {output.name}\n"
        f"Source/config files embedded: {len(sources)}\n"
        + "\n".join(relative(p) for p in sources)
        + f"\n\nCI status: {os.environ.get('REPORT_CTEST_STATUS', 'not supplied')}\n"
        + f"Commit: {os.environ.get('REPORT_COMMIT_SHA', os.environ.get('GITHUB_SHA', 'not supplied'))}\n"
        + f"CI run: {os.environ.get('REPORT_CI_RUN_URL', 'not supplied')}\n",
        encoding="utf-8",
    )
    print(f"Generated: {output}")
    print(f"Size: {output.stat().st_size} bytes")
    print(f"Embedded source/config files: {len(sources)}")
    print(f"Manifest: {manifest}")
    print("Structural validation: PASS")
    print("Visual layout review: requires artifact inspection")
    print(f"CTest/CI status: {os.environ.get('REPORT_CTEST_STATUS', 'not supplied')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
