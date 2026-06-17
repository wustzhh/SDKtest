#pragma once

#include <QString>
#include <QVector>
#include <QStringList>
#include <QByteArray>

// ────────────────────────────────────────────────────────────
//  零依赖 XLSX 生成器（STORE 压缩，内联字符串）
//  输出标准的 Office Open XML .xlsx
// ────────────────────────────────────────────────────────────
class XlsxWriter {
public:
    XlsxWriter();

    // 添加一个工作表
    void addSheet(const QString& sheetName,
                  const QStringList& headers,
                  const QVector<QStringList>& rows);

    // 保存到文件
    bool save(const QString& path);

    // 错误信息
    QString lastError() const { return m_lastError; }

private:
    struct Sheet {
        QString name;
        QStringList headers;
        QVector<QStringList> rows;
    };

    QVector<Sheet> m_sheets;
    mutable QString m_lastError;

    // ── ZIP 工具 ──
    static QByteArray buildZip(const QVector<QPair<QString, QByteArray>>& files);
    static QByteArray crc32(const QByteArray& data);

    // ── XML 构建 ──
    QByteArray buildContentTypes() const;
    QByteArray buildRels() const;
    QByteArray buildWorkbook() const;
    QByteArray buildWorkbookRels() const;
    QByteArray buildStyles() const;
    QByteArray buildSheet(int index) const;

    static QByteArray toBytes(quint32 val, bool littleEndian = true);
    static QByteArray toBytes(quint16 val, bool littleEndian = true);
};
