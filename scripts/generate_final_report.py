#!/usr/bin/env python3
"""Generate a coursework report DOCX from the current repository checkout.

This generator deliberately avoids claiming that tests passed: test outcomes must be
verified from a real CTest/CI run and are described as unverified unless supplied.
"""
from __future__ import annotations

import argparse
import re
import sys
import zipfile
from pathlib import Path
from typing import Iterable

try:
    from docx import Document
    from docx.enum.section import WD_SECTION
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
    """Describe the role of a parameter from its declared name/type, without inventing values."""
    normalized = re.sub(r"\s+", " ", argument.strip())
    name_match = re.search(r"([A-Za-z_]\w*)\s*(?:=\s*[^,]+)?$", normalized)
    name = name_match.group(1) if name_match else "параметр"
    lower = name.lower()
    if name in {"", "void"}:
        role = "параметр отсутствует"
    elif any(word in lower for word in ("path", "directory", "folder", "root")):
        role = "путь/каталог, задающий объект файловой системы или область операции"
    elif any(word in lower for word in ("file", "filename", "filepath")):
        role = "файл или имя файла, обрабатываемое операцией"
    elif any(word in lower for word in ("size", "bytes", "count", "total")):
        role = "числовое значение размера или количества"
    elif any(word in lower for word in ("category", "type", "format", "status", "scope")):
        role = "параметр выбора категории, типа, формата, статуса или области"
    elif any(word in lower for word in ("node", "parent", "child", "directorynode")):
        role = "узел дерева каталогов или связь между узлами"
    elif any(word in lower for word in ("result", "statistics", "stats", "data", "info")):
        role = "данные результата или статистики, используемые операцией"
    elif any(word in lower for word in ("cancel", "stop", "running")):
        role = "состояние или запрос управления выполнением"
    elif any(word in lower for word in ("progress", "callback", "handler", "observer")):
        role = "функция обратного вызова/получатель уведомлений"
    elif any(word in lower for word in ("error", "message", "reason")):
        role = "описание ошибки или диагностическое сообщение"
    elif lower == "argc":
        role = "количество аргументов командной строки при запуске приложения"
    elif lower == "argv":
        role = "массив аргументов командной строки при запуске приложения"
    elif any(word in lower for word in ("index", "row", "column", "level")):
        role = "индекс, позиция или уровень элемента"
    else:
        role = "входное значение, необходимое для выполнения операции"
    if "&&" in normalized:
        mode = "входной параметр с передачей владения/перемещением (rvalue-ссылка)"
        direction = "входной"
    elif "&" in normalized and "const" in normalized:
        mode = "константная ссылка"
        direction = "входной"
    elif "&" in normalized:
        mode = "неконстантная ссылка"
        direction = "входной-выходной; может быть выходным, если тело метода записывает в параметр"
    elif "*" in normalized:
        mode = "указатель"
        direction = "входной или входной-выходной; nullability и направление определяются контрактом реализации"
    else:
        mode = "значение/копия"
        direction = "входной"
    return f"{normalized} — роль: {role}; направление: {direction}; способ передачи: {mode}."


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
        return f"Структура данных «{name}», объединяющая связанные поля предметной области; точный состав указан в объявлении."
    if kind == "class":
        return f"Класс «{name}», инкапсулирующий данные и операции соответствующего компонента; публичные операции перечислены ниже."
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
    if re.search(r"\b(?:m_[A-Za-z_]\w*|this->\w+)\s*(?:=|\+=|-=|\+\+|--|\.push_back|\.emplace_back|\.clear|\.insert|\.erase)", body):
        effects.append("изменяет состояние объекта или принадлежащую ему структуру данных")
    if not effects:
        if "const" in declaration:
            effects.append("в теле не выявлены очевидные записи во внешние ресурсы или поля объекта")
        else:
            effects.append("явные внешние записи не обнаружены; возможные изменения локального/внутреннего состояния следует проверять по полному телу")
    return "; ".join(effects) + "."


def extract_program_units(files: list[Path]) -> dict[str, list[list[str]]]:
    """Extract declarations from current headers and produce documented unit cards."""
    groups: dict[str, list[list[str]]] = {
        "Классы": [],
        "Структуры": [],
        "Интерфейсы": [],
        "Перечисления и псевдонимы": [],
        "Функции и методы": [],
    }
    type_pattern = re.compile(r"\b(class|struct|enum\s+class|enum|using|typedef)\s+([A-Za-z_][A-Za-z0-9_]*)")
    seen_types: set[tuple[str, str]] = set()
    seen_functions: set[tuple[str, str, str]] = set()
    for path in files:
        if path.suffix.lower() not in {".h", ".hpp"}:
            continue
        text = read_text(path)
        for match in type_pattern.finditer(text):
            kind, name = match.group(1), match.group(2)
            if name in {"if", "while", "switch"}:
                continue
            if kind in {"class", "struct"}:
                opening = text.find("{", match.end())
                semicolon = text.find(";", match.end())
                if opening < 0 or (semicolon >= 0 and semicolon < opening):
                    continue
            key = (kind, name)
            if key in seen_types:
                continue
            seen_types.add(key)
            if kind.startswith("enum") or kind in {"using", "typedef"}:
                group = "Перечисления и псевдонимы"
            elif kind == "struct":
                group = "Структуры"
            elif name.startswith("I") or "/interfaces/" in relative(path).replace("\\", "/"):
                group = "Интерфейсы"
            else:
                group = "Классы"
            description = _describe_unit(name, kind, path)
            if kind.startswith("enum"):
                body_start = text.find("{", match.end())
                body_end = text.find("}", body_start + 1) if body_start >= 0 else -1
                members = re.sub(r"\s+", " ", text[body_start + 1:body_end]).strip() if body_start >= 0 and body_end > body_start else "состав не извлечён автоматически"
                signature = f"{kind} {name} {{ {members} }}"
                output = "Набор именованных значений перечисления; отдельного возвращаемого значения нет."
            elif kind in {"using", "typedef"}:
                line_end = text.find(";", match.start())
                signature = re.sub(r"\s+", " ", text[match.start():line_end + 1]).strip() if line_end >= 0 else f"{kind} {name}"
                output = "Псевдоним типа на этапе компиляции; во время выполнения данных не создаёт."
            else:
                members = _extract_type_members(text, match)
                signature = f"{kind} {name}"
                if members:
                    signature += "; поля/состав: " + " | ".join(members)
                output = "Не возвращает значение как функция; при создании экземпляра формируется объект этого типа."
            groups[group].append([
                name, signature, "Аргументы конструктора/методов описываются отдельно в таблице функций и методов.",
                description, output,
                "Само объявление тип не изменяет; изменение состояния возможно при создании экземпляров и вызове его методов.",
                relative(path),
            ])

        # Parse declarations terminated by ';' (including multiline declarations).
        # Definitions in .cpp are intentionally not guessed: public/private signatures are taken from headers.
        for match in re.finditer(r"(?m)^[ \t]*(?:virtual\s+|static\s+|inline\s+|explicit\s+|constexpr\s+|friend\s+|Q_INVOKABLE\s+|Q_SLOT\s+|Q_SIGNAL\s+)*(?:[\w:~<>]+(?:\s*[*&]+)?(?:\s+|\s*[*&]\s*)+)?[A-Za-z_~]\w*\s*\([^;{}]*\)\s*(?:const\s*)?(?:noexcept\s*)?(?:override\s*)?(?:final\s*)?(?:=\s*0\s*)?;", text):
            declaration = re.sub(r"\s+", " ", match.group(0)).strip()
            if declaration.startswith(("if ", "for ", "while ", "switch ")):
                continue
            name_match = re.search(r"([A-Za-z_~]\w*)\s*\((.*)\)", declaration)
            if not name_match:
                continue
            name, params_text = name_match.group(1), name_match.group(2)
            if name in {"if", "for", "while", "switch", "static_assert"}:
                continue
            key = (relative(path), name, declaration)
            if key in seen_functions:
                continue
            seen_functions.add(key)
            args = _split_parameters(params_text)
            arg_description = "\n".join(_argument_description(arg) for arg in args) if args else "Нет аргументов."
            return_match = re.match(r"(.*?)(?:\b" + re.escape(name) + r"\s*\()", declaration)
            return_type = return_match.group(1).strip() if return_match else "тип не определён автоматически"
            return_type = re.sub(r"^(?:(?:virtual|static|inline|explicit|constexpr|friend|Q_INVOKABLE|Q_SLOT|Q_SIGNAL)\s+)+", "", return_type).strip()
            if name.startswith("~") or name == Path(relative(path)).stem:
                return_type = "конструктор/деструктор"
            output = f"Тип результата: {return_type or 'void/не определён'}."
            if not return_type or return_type == "void":
                output += " Возвращаемое значение отсутствует."
            elif "конструктор" in return_type:
                output += " Создаёт или завершает время жизни экземпляра."
            else:
                output += " Возвращает значение указанного типа; конкретный смысл определяется реализацией метода."
            body = _find_function_body(name, files, Path(relative(path)).stem)
            effects_text = _describe_effects(name, declaration, body)
            groups["Функции и методы"].append([
                name, declaration, arg_description,
                _describe_function(name, declaration, path),
                output, effects_text, relative(path),
            ])
    # main() is the application's composition root and is intentionally listed as a program unit.
    main_path = next((path for path in files if relative(path) == "src/main.cpp"), None)
    if main_path is not None:
        main_text = read_text(main_path)
        main_match = re.search(r"(?s)\bint\s+main\s*\((.*?)\)\s*\{", main_text)
        if main_match:
            params = _split_parameters(main_match.group(1))
            args = "\n".join(_argument_description(arg) for arg in params) if params else "Нет аргументов."
            groups["Функции и методы"].append([
                "main", "int main(" + ", ".join(params) + ")",
                args,
                "Точка входа: создаёт QApplication, собирает зависимости сканера, классификатора, агрегатора, конвейера, контроллера и главного окна; запускает цикл обработки событий Qt.",
                "Возвращает целочисленный код завершения приложения, полученный от цикла событий Qt.",
                "Создаёт объекты приложения и UI, запускает цикл событий; само по себе не выполняет анализ каталога до команды пользователя.",
                relative(main_path),
            ])
    for group in groups:
        groups[group].sort(key=lambda row: (row[0].lower(), row[-1].lower()))
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
        paragraph.add_run("\\t")
        elements.append(paragraph._p)

    page_break = doc.add_paragraph()
    page_break.add_run().add_break()
    page_break.paragraph_format.page_break_before = True
    elements.append(page_break._p)

    # Move the newly created paragraphs from the document end to the front matter.
    for element in elements:
        assignment.addprevious(element)



def add_title_page(doc: Document) -> None:
    for line in [
        "МИНИСТЕРСТВО НАУКИ И ВЫСШЕГО ОБРАЗОВАНИЯ РОССИЙСКОЙ ФЕДЕРАЦИИ",
        "Автономная некоммерческая организация высшего образования",
        "«МОСКОВСКИЙ АВИАЦИОННЫЙ ИНСТИТУТ",
        "(национальный исследовательский университет)»",
        "Институт № 7 «Интеллектуальные и робототехнические системы»",
        "Кафедра 703",
    ]:
        p = doc.add_paragraph()
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        r = p.add_run(line)
        r.font.name = "Times New Roman"
        r.font.size = Pt(12 if len(line) < 80 else 10)
        if line.startswith("«МОСКОВСКИЙ"):
            r.bold = True
    for _ in range(3):
        doc.add_paragraph()
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    r = p.add_run("КУРСОВАЯ РАБОТА")
    r.bold = True
    r.font.size = Pt(16)
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.add_run("по дисциплине «Технологии разработки программного обеспечения»")
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    r = p.add_run("Разработка оконного приложения для анализа дискового пространства файловой системы")
    r.bold = True
    r.font.size = Pt(14)
    for _ in range(3):
        doc.add_paragraph()
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.RIGHT
    p.add_run("Выполнил: студент группы М70-407С-22\nОсипенко Гавриил Васильевич\n\nПроверил: Барчев Н. Б.")
    for _ in range(3):
        doc.add_paragraph()
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.add_run("Москва, 2026")
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
    doc.add_heading("Сведения о программной реализации", level=1)
    doc.add_heading("Сведения о выбранном языке программирования", level=2)
    add_paragraph(doc, "Программа реализована на C++17. Для оконного интерфейса и визуализации используются Qt 5 Core, Widgets и Charts. Сборка конфигурируется CMake, автоматические тесты регистрируются через CTest, CI выполняется GitHub Actions.")
    doc.add_heading("Описание входных и выходных данных", level=2)
    add_table(doc, ["Вид", "Данные", "Назначение"], [
        ["Вход", "Путь к корневому каталогу", "Определяет область анализа."],
        ["Вход", "Состояние файловой системы и права доступа", "Влияют на доступность объектов и полноту результата."],
        ["Вход", "Команды интерфейса", "Запуск/отмена анализа, выбор каталога, переключение области статистики, экспорт."],
        ["Внутренние данные", "Путь, тип, размер и категория объекта при обработке", "Используются сканером, классификатором и агрегатором согласно текущим контрактам."],
        ["Выход", "Дерево каталогов и агрегированная статистика", "Навигация и анализ по директориям без повторного полного обхода."],
        ["Выход", "Таблица, круговая диаграмма, прогресс и ошибки", "Представление результата пользователю."],
        ["Выход", "JSON или TXT", "Сохранение агрегированного результата."],
    ])
    doc.add_heading("Описание спроектированных структур и программных единиц", level=2)
    add_paragraph(doc, "Программные единицы сгруппированы по видам. Для типов указаны состав и назначение; для функций и методов — сигнатура, роль и способ передачи каждого аргумента, тип выходных данных и возможное влияние на состояние или внешние ресурсы. Сигнатуры извлекаются из заголовочных файлов текущего checkout. Для точного описания побочных эффектов и назначения каждой операции необходимо учитывать тело реализации, а не только объявление.")
    units = extract_program_units(source)
    for group_name in ("Классы", "Структуры", "Интерфейсы", "Перечисления и псевдонимы", "Функции и методы"):
        doc.add_heading(group_name, level=3)
        rows = units[group_name]
        if rows:
            add_table(doc, ["Программная единица и сигнатура / состав", "Аргументы: роль и описание", "Назначение", "Выходные данные и изменение состояния / внешних данных", "Файл"], [[row[0] + "\n" + row[1], row[2], row[3], row[4] + "\n\n" + row[5], row[6]] for row in rows], [3.4, 3.6, 3.3, 4.0, 2.0])
        else:
            add_paragraph(doc, "В текущем исходном коде программные единицы этой группы не обнаружены.")

    doc.add_heading("Архитектура и потоки данных", level=2)
    add_table(doc, ["Компонент", "Ответственность", "Связи/поток данных"], [
        ["Domain", "Модели, перечисления и интерфейсы предметной области.", "Используется прикладным и инфраструктурным слоями; не зависит от Qt."],
        ["FileClassifier", "Классификация файлов по расширению.", "Сканер/reader используют результат классификации."],
        ["DirectoryScanner", "Итеративный обход, построение дерева, Direct-статистика.", "Передаёт построенное дерево агрегатору."],
        ["DirectoryAggregator", "Агрегация Subtree/Total снизу вверх.", "Работает с деревом в памяти; не выполняет файловый ввод-вывод."],
        ["AnalysisPipeline", "Последовательность сканирования и агрегации.", "Вызывается контроллером."],
        ["ApplicationController", "Фоновая задача, отмена, прогресс, ошибки и передача результата.", "Связывает логику приложения с UI."],
        ["DirectoryContentsReader / DirectoryCacheManager", "Список непосредственного содержимого и кэш для навигации.", "Поддерживают отображение выбранного каталога; не владеют деревом результата."],
        ["VisualizationModel / MainWindow", "Подготовка данных диаграммы и GUI.", "Показывают статистику и команды пользователя."],
        ["ExportManager", "Сериализация агрегированных данных.", "Записывает JSON/TXT."],
    ])
    doc.add_heading("Инструкция пользователя", level=2)
    add_numbered(doc, [
        "Запустить переносимую сборку для Windows x64 или Linux x86_64 либо собрать проект из исходного кода.",
        "Указать путь к каталогу или выбрать каталог в диалоге.",
        "Запустить анализ и дождаться завершения; при необходимости отменить задачу.",
        "Выбирать каталоги в дереве и просматривать статистику.",
        "Переключать область диаграммы: Direct («Только здесь»), Subtree («Вложенные каталоги») или Total («Весь каталог») — сверить точные подписи с актуальным UI.",
        "При необходимости экспортировать результат в JSON или TXT.",
        "После изменения файловой системы повторить полный анализ. Обновление списка непосредственного содержимого само по себе не пересчитывает всё дерево.",
    ])
    doc.add_heading("Состав исходных файлов", level=2)
    add_paragraph(doc, f"В листинг включено файлов: {len(source)}. Полный перечень приведён в приложении «Листинг».")
    for p in source:
        add_paragraph(doc, relative(p))


def add_testing(doc: Document, source: list[Path]) -> None:
    doc.add_heading("Тестирование", level=1)
    test_files = sorted((ROOT / "tests").glob("*.cpp")) if (ROOT / "tests").exists() else []
    doc.add_heading("Автоматические тесты в репозитории", level=2)
    if test_files:
        rows = []
        for p in test_files:
            rows.append([p.stem, relative(p), "Тестовый исходник присутствует. Результат запуска не утверждается генератором; проверить CTest/CI для выбранного commit."])
        add_table(doc, ["Тест", "Файл", "Фактический статус"], rows)
    else:
        add_paragraph(doc, "Тестовые файлы .cpp не обнаружены в tests/. Проверить конфигурацию тестирования.")
    doc.add_paragraph("Для подтверждения прохождения необходимо выполнить CMake build и CTest, затем зафиксировать ссылку на конкретный успешный запуск GitHub Actions. Наличие тестового исходника не является доказательством успешного выполнения.")
    doc.add_heading("Сценарии проверки", level=2)
    scenarios = [
        ["Путь к существующему каталогу с файлами разных расширений", "Анализ завершается; дерево, статистика по размерам и категориям отображаются.", "Требует запуска приложения и сверки с фактическими размерами тестовых файлов; в генераторе не выполнялся."],
        ["Каталог с вложенными папками и файлами на обоих уровнях", "Direct отражает только непосредственные файлы; Subtree — файлы вложенных каталогов; Total равен их сумме.", "Требует ручной приёмки или отдельного подтверждённого теста."],
        ["Пустой каталог", "Размеры и доли не приводят к делению на ноль; пустая статистика отображается корректно.", "Требует фактического прогона."],
        ["Каталог без прав чтения / недоступный объект", "Ошибка фиксируется; доступные части обрабатываются, результат помечается согласно статусу/ошибкам реализации.", "Требует проверки на целевой ОС."],
        ["Отмена анализа во время обхода", "Запрос отмены обрабатывается; результат и статус согласованы.", "Проверять тест ApplicationControllerTest и/или CI; конкретный результат здесь не известен."],
        ["Выбор разных каталогов в дереве", "Статистика и диаграмма соответствуют выбранному узлу без полного повторного сканирования.", "Требует ручного GUI-теста."],
        ["Экспорт в JSON и TXT", "Создаются файлы выбранного формата с агрегированными данными.", "Проверять ExportManagerTest/CTest; конкретный результат здесь не известен."],
        ["Символическая ссылка на каталог", "Цель не обходится рекурсивно; не возникает циклического обхода.", "Требует проверки на ОС с поддержкой символических ссылок."],
    ]
    add_table(doc, ["Вводимое значение", "Предполагаемая реакция", "Действительная реакция"], scenarios)
    doc.add_heading("Итог тестирования", level=2)
    add_paragraph(doc, "Генератор не запускает приложение и не подменяет результаты CTest или ручной приёмки. Перед сдачей отчёта необходимо открыть GitHub Actions для commit, из которого создан документ, записать фактический статус сборки и тестов, а сценарии без подтверждения оставить помеченными как требующие проверки. Smoke-test запуска GUI не равнозначен проверке всех функций интерфейса.")


def add_listing(doc: Document, source: list[Path]) -> None:
    doc.add_heading("Листинг", level=1)
    add_paragraph(doc, "Ниже приведены полные тексты файлов из текущего checkout. Если код изменится, документ необходимо сгенерировать повторно.")
    for index, path in enumerate(source, start=1):
        doc.add_heading(f"{index}. {relative(path)}", level=2)
        text = read_text(path)
        if not text.strip():
            add_paragraph(doc, "[Файл пуст]")
        else:
            add_code_block(doc, text, as_table=True)
        doc.add_paragraph()


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
    insert_static_table_of_contents(doc)

    doc.core_properties.title = "Отчёт по курсовой работе: Disk Analyzer"
    doc.core_properties.subject = "ТРПО, C++17, Qt 5"
    doc.core_properties.author = "Осипенко Гавриил Васильевич"
    doc.save(output)

    expected = ["Оглавление", "Задание", "Псевдокод алгоритмов решения задачи", "Сведения о программной реализации", "Классы", "Структуры", "Интерфейсы", "Перечисления и псевдонимы", "Функции и методы", "Листинг", "Тестирование", "Заключение"]
    validate_docx(output, expected, len(sources))
    manifest = output.with_suffix(".manifest.txt")
    manifest.write_text(
        "Generated report inventory\n"
        f"Output: {output.name}\n"
        f"Source/config files embedded: {len(sources)}\n"
        + "\n".join(relative(p) for p in sources)
        + "\n\nReminder: visual review and CI result verification are still required.\n",
        encoding="utf-8",
    )
    print(f"Generated: {output}")
    print(f"Size: {output.stat().st_size} bytes")
    print(f"Embedded source/config files: {len(sources)}")
    print(f"Manifest: {manifest}")
    print("Structural validation: PASS")
    print("Visual layout review: NOT PERFORMED by this script")
    print("CTest/CI status: NOT queried by this script")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
