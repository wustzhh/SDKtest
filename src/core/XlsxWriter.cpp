#include "XlsxWriter.h"

#include <QFile>
#include <QXmlStreamWriter>
#include <QBuffer>
#include <QDateTime>

// ════════════════════════════════════════════════════════════
//  XlsxWriter 实现
// ════════════════════════════════════════════════════════════

XlsxWriter::XlsxWriter() {}

void XlsxWriter::addSheet(const QString& sheetName,
                           const QStringList& headers,
                           const QVector<QStringList>& rows)
{
    m_sheets.push_back({ sheetName, headers, rows });
}

// ────────────────────────────────────────────────────────────
//  ZIP 打包（STORE 方法，无压缩）
// ────────────────────────────────────────────────────────────

QByteArray XlsxWriter::crc32(const QByteArray& data) {
    static quint32 table[256];
    static bool init = false;
    if (!init) {
        for (quint32 i = 0; i < 256; i++) {
            quint32 crc = i;
            for (int j = 0; j < 8; j++)
                crc = (crc >> 1) ^ (crc & 1 ? 0xEDB88320 : 0);
            table[i] = crc;
        }
        init = true;
    }
    quint32 crc = 0xFFFFFFFF;
    for (int i = 0; i < data.size(); i++)
        crc = table[(crc ^ (quint8)data[i]) & 0xFF] ^ (crc >> 8);
    return toBytes(crc ^ 0xFFFFFFFF);
}

QByteArray XlsxWriter::toBytes(quint32 val, bool littleEndian) {
    QByteArray b(4, '\0');
    if (littleEndian) {
        b[0] = val & 0xFF;
        b[1] = (val >> 8) & 0xFF;
        b[2] = (val >> 16) & 0xFF;
        b[3] = (val >> 24) & 0xFF;
    } else {
        b[3] = val & 0xFF;
        b[2] = (val >> 8) & 0xFF;
        b[1] = (val >> 16) & 0xFF;
        b[0] = (val >> 24) & 0xFF;
    }
    return b;
}

QByteArray XlsxWriter::toBytes(quint16 val, bool littleEndian) {
    QByteArray b(2, '\0');
    if (littleEndian) {
        b[0] = val & 0xFF;
        b[1] = (val >> 8) & 0xFF;
    } else {
        b[1] = val & 0xFF;
        b[0] = (val >> 8) & 0xFF;
    }
    return b;
}

QByteArray XlsxWriter::buildZip(const QVector<QPair<QString, QByteArray>>& files) {
    QByteArray zip;
    QVector<quint32> offsets;
    QVector<QByteArray> crcs;

    // ── Local file entries ──
    for (const auto& [name, data] : files) {
        offsets.push_back(zip.size());
        QByteArray nameBytes = name.toUtf8();
        QByteArray crc = crc32(data);

        zip += QByteArray("PK\x03\x04", 4);  // signature
        zip += toBytes((quint16)20);          // version needed
        zip += toBytes((quint16)0);           // flags
        zip += toBytes((quint16)0);           // compression (STORE)
        zip += toBytes((quint16)0);           // mod time
        zip += toBytes((quint16)0);           // mod date
        zip += crc;                           // crc32
        zip += toBytes((quint32)data.size()); // compressed size
        zip += toBytes((quint32)data.size()); // uncompressed size
        zip += toBytes((quint16)nameBytes.size()); // filename length
        zip += toBytes((quint16)0);           // extra field length
        zip += nameBytes;
        zip += data;
        crcs.push_back(crc);
    }

    // ── Central directory ──
    quint32 cdOffset = zip.size();
    for (int i = 0; i < files.size(); i++) {
        QByteArray nameBytes = files[i].first.toUtf8();
        quint32 uncompressedSize = files[i].second.size();

        zip += QByteArray("PK\x01\x02", 4);  // signature
        zip += toBytes((quint16)20);          // version made by
        zip += toBytes((quint16)20);          // version needed
        zip += toBytes((quint16)0);           // flags
        zip += toBytes((quint16)0);           // compression
        zip += toBytes((quint16)0);           // mod time
        zip += toBytes((quint16)0);           // mod date
        zip += crcs[i];                       // crc32
        zip += toBytes((quint32)uncompressedSize);
        zip += toBytes((quint32)uncompressedSize);
        zip += toBytes((quint16)nameBytes.size());
        zip += toBytes((quint16)0);           // extra field length
        zip += toBytes((quint16)0);           // file comment length
        zip += toBytes((quint16)0);           // disk number start
        zip += toBytes((quint16)0);           // internal attrs
        zip += toBytes((quint32)0);           // external attrs
        zip += toBytes((quint32)offsets[i]);  // relative offset
        zip += nameBytes;
    }

    // ── End of central directory ──
    quint32 cdSize = zip.size() - cdOffset;
    zip += QByteArray("PK\x05\x06", 4);      // signature
    zip += toBytes((quint16)0);               // disk #
    zip += toBytes((quint16)0);               // CD disk #
    zip += toBytes((quint16)files.size());    // entries on disk
    zip += toBytes((quint16)files.size());    // total entries
    zip += toBytes((quint32)cdSize);          // CD size
    zip += toBytes((quint32)cdOffset);        // CD offset
    zip += toBytes((quint16)0);               // comment length

    return zip;
}

// ────────────────────────────────────────────────────────────
//  XML 构建辅助
// ────────────────────────────────────────────────────────────
static QByteArray xmlStr(const QString& str) {
    QString escaped(str);
    escaped.replace("&", "&amp;");
    escaped.replace("<", "&lt;");
    escaped.replace(">", "&gt;");
    escaped.replace("\"", "&quot;");
    escaped.replace("'", "&apos;");
    return escaped.toUtf8();
}

QByteArray XlsxWriter::buildContentTypes() const {
    QByteArray xml;
    xml += "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";
    xml += "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n";
    xml += "  <Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n";
    xml += "  <Default Extension=\"xml\" ContentType=\"application/xml\"/>\n";
    xml += "  <Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>\n";
    xml += "  <Override PartName=\"/xl/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>\n";
    for (int i = 0; i < m_sheets.size(); i++) {
        xml += QString("  <Override PartName=\"/xl/worksheets/sheet%1.xml\" "
                       "ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n")
                   .arg(i + 1).toUtf8();
    }
    xml += "</Types>\n";
    return xml;
}

QByteArray XlsxWriter::buildRels() const {
    return QByteArray(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
        "  <Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>\n"
        "</Relationships>\n");
}

QByteArray XlsxWriter::buildWorkbook() const {
    QByteArray xml;
    xml += "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";
    xml += "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
           "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n";
    xml += "  <sheets>\n";
    for (int i = 0; i < m_sheets.size(); i++) {
        xml += QString("    <sheet name=\"%1\" sheetId=\"%2\" r:id=\"rId%3\"/>\n")
                   .arg(xmlStr(m_sheets[i].name)).arg(i + 1).arg(i + 1).toUtf8();
    }
    xml += "  </sheets>\n";
    xml += "</workbook>\n";
    return xml;
}

QByteArray XlsxWriter::buildWorkbookRels() const {
    QByteArray xml;
    xml += "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";
    xml += "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n";
    xml += "  <Relationship Id=\"rIdStyles\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>\n";
    for (int i = 0; i < m_sheets.size(); i++) {
        xml += QString("  <Relationship Id=\"rId%1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet%1.xml\"/>\n")
                   .arg(i + 1).toUtf8();
    }
    xml += "</Relationships>\n";
    return xml;
}

QByteArray XlsxWriter::buildStyles() const {
    return QByteArray(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n"
        "  <fonts count=\"3\">\n"
        "    <font><sz val=\"11\"/><name val=\"Microsoft YaHei\"/></font>\n"
        "    <font><b/><sz val=\"11\"/><color rgb=\"FFFFFFFF\"/><name val=\"Microsoft YaHei\"/></font>\n"
        "    <font><sz val=\"11\"/><color rgb=\"FF333333\"/><name val=\"Microsoft YaHei\"/></font>\n"
        "  </fonts>\n"
        "  <fills count=\"5\">\n"
        "    <fill><patternFill patternType=\"none\"/></fill>\n"
        "    <fill><patternFill patternType=\"gray125\"/></fill>\n"
        "    <fill><patternFill patternType=\"solid\"><fgColor rgb=\"FF4CAF50\"/></patternFill></fill>\n"
        "    <fill><patternFill patternType=\"solid\"><fgColor rgb=\"FFF44336\"/></patternFill></fill>\n"
        "    <fill><patternFill patternType=\"solid\"><fgColor rgb=\"FF4472C4\"/></patternFill></fill>\n"
        "  </fills>\n"
        "  <borders count=\"2\">\n"
        "    <border><left/><right/><top/><bottom/><diagonal/></border>\n"
        "    <border>\n"
        "      <left style=\"thin\"><color auto=\"1\"/></left>\n"
        "      <right style=\"thin\"><color auto=\"1\"/></right>\n"
        "      <top style=\"thin\"><color auto=\"1\"/></top>\n"
        "      <bottom style=\"thin\"><color auto=\"1\"/></bottom>\n"
        "      <diagonal/>\n"
        "    </border>\n"
        "  </borders>\n"
        "  <cellStyleXfs count=\"1\">\n"
        "    <xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/>\n"
        "  </cellStyleXfs>\n"
        "  <cellXfs count=\"5\">\n"
        "    <xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/>\n"
        "    <xf numFmtId=\"0\" fontId=\"1\" fillId=\"4\" borderId=\"1\" applyFont=\"1\" applyFill=\"1\" applyBorder=\"1\"/>\n"
        "    <xf numFmtId=\"0\" fontId=\"0\" fillId=\"2\" borderId=\"1\" applyFill=\"1\" applyBorder=\"1\"/>\n"
        "    <xf numFmtId=\"0\" fontId=\"0\" fillId=\"3\" borderId=\"1\" applyFill=\"1\" applyBorder=\"1\"/>\n"
        "    <xf numFmtId=\"0\" fontId=\"2\" fillId=\"0\" borderId=\"0\"/>\n"
        "  </cellXfs>\n"
        "</styleSheet>\n");
}

QByteArray XlsxWriter::buildSheet(int index) const {
    if (index < 0 || index >= m_sheets.size()) return {};
    const auto& sheet = m_sheets[index];

    // xlsx style IDs: 0=normal, 1=header(bold+border), 2=pass(green), 3=fail(red)
    auto cellStyle = [](const QString& text) -> int {
        QString t = text.toUpper().trimmed();
        if (t == "PASSED" || t == "PASS" || t == "✓" || t == "OK") return 2;
        if (t == "FAILED" || t == "FAIL" || t == "✗" || t == "ERROR") return 3;
        return 0;
    };

    QByteArray xml;
    xml += "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";
    xml += "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n";
    xml += "  <cols>\n";
    xml += "    <col min=\"1\" max=\"26\" width=\"30\" customWidth=\"1\"/>\n";
    xml += "  </cols>\n";
    xml += "  <sheetData>\n";

    // 行从 1 开始
    int rowNum = 1;

    // ── Header row ──
    xml += QString("    <row r=\"%1\">").arg(rowNum).toUtf8();
    int col = 1;
    for (const auto& h : sheet.headers) {
        QString colRef = QChar('A' + col - 1) + QString::number(rowNum);
        xml += QString("<c r=\"%1\" t=\"inlineStr\" s=\"1\"><is><t>%2</t></is></c>")
                   .arg(colRef, xmlStr(h)).toUtf8();
        col++;
    }
    xml += "</row>\n";
    rowNum++;

    // ── Data rows ──
    for (const auto& row : sheet.rows) {
        xml += QString("    <row r=\"%1\">").arg(rowNum).toUtf8();
        col = 1;
        for (const auto& cell : row) {
            QString colRef = QChar('A' + col - 1) + QString::number(rowNum);
            int style = cellStyle(cell);
            xml += QString("<c r=\"%1\" t=\"inlineStr\" s=\"%2\"><is><t>%3</t></is></c>")
                       .arg(colRef).arg(style).arg(xmlStr(cell)).toUtf8();
            col++;
        }
        xml += "</row>\n";
        rowNum++;
    }

    xml += "  </sheetData>\n";
    xml += "</worksheet>\n";
    return xml;
}

bool XlsxWriter::save(const QString& path) {
    if (m_sheets.isEmpty()) {
        m_lastError = "No sheets to write.";
        return false;
    }

    QVector<QPair<QString, QByteArray>> files;

    // 固定结构
    files << qMakePair(QString("[Content_Types].xml"), buildContentTypes());
    files << qMakePair(QString("_rels/.rels"), buildRels());
    files << qMakePair(QString("xl/workbook.xml"), buildWorkbook());
    files << qMakePair(QString("xl/_rels/workbook.xml.rels"), buildWorkbookRels());
    files << qMakePair(QString("xl/styles.xml"), buildStyles());

    for (int i = 0; i < m_sheets.size(); i++) {
        files << qMakePair(QString("xl/worksheets/sheet%1.xml").arg(i + 1),
                           buildSheet(i));
    }

    QByteArray zipData = buildZip(files);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        m_lastError = QString("Cannot write '%1': %2").arg(path, file.errorString());
        return false;
    }
    file.write(zipData);
    file.close();
    return true;
}
