#include "Filter.h"

#include <stdexcept>

namespace {

enum class Op {
    Eq,
    Ne,
    Gt,
    Ge,
    Lt,
    Le,
    Like,
};

struct Condition {
    int column = -1;
    Op op = Op::Eq;
    QString value;
};

QString unquote(QString value)
{
    value = value.trimmed();
    if (value.size() >= 2) {
        const QChar first = value.front();
        const QChar last = value.back();
        if ((first == QLatin1Char('\'') && last == QLatin1Char('\'')) || (first == QLatin1Char('"') && last == QLatin1Char('"')))
            return value.mid(1, value.size() - 2);
    }
    return value;
}

QStringList splitKeyword(const QString &expr, const QString &keyword)
{
    QStringList parts;
    QString current;
    bool quoted = false;
    QChar quote;
    const QString needle = QStringLiteral(" %1 ").arg(keyword);

    for (int i = 0; i < expr.size();) {
        const QChar ch = expr[i];
        if (quoted) {
            if (ch == quote)
                quoted = false;
            current.append(ch);
            ++i;
            continue;
        }
        if (ch == QLatin1Char('\'') || ch == QLatin1Char('"')) {
            quoted = true;
            quote = ch;
            current.append(ch);
            ++i;
            continue;
        }
        if (expr.mid(i, needle.size()).compare(needle, Qt::CaseInsensitive) == 0) {
            parts.append(current.trimmed());
            current.clear();
            i += needle.size();
            continue;
        }
        current.append(ch);
        ++i;
    }
    if (!current.trimmed().isEmpty())
        parts.append(current.trimmed());
    return parts;
}

Condition parseCondition(const TableData &table, const QString &part)
{
    const QList<QPair<QString, Op>> ops = {
        {QStringLiteral(">="), Op::Ge},
        {QStringLiteral("<="), Op::Le},
        {QStringLiteral("<>"), Op::Ne},
        {QStringLiteral(" LIKE "), Op::Like},
        {QStringLiteral("="), Op::Eq},
        {QStringLiteral(">"), Op::Gt},
        {QStringLiteral("<"), Op::Lt},
    };

    for (const auto &item : ops) {
        const int pos = part.indexOf(item.first, 0, Qt::CaseInsensitive);
        if (pos <= 0)
            continue;

        const QString field = part.left(pos).trimmed();
        int column = -1;
        for (int i = 0; i < table.columns.size(); ++i) {
            if (table.columns[i].name.compare(field, Qt::CaseInsensitive) == 0) {
                column = i;
                break;
            }
        }
        if (column < 0)
            throw std::runtime_error(QStringLiteral("Unknown field: %1").arg(field).toStdString());

        return Condition{column, item.second, unquote(part.mid(pos + item.first.size()))};
    }

    throw std::runtime_error(QStringLiteral("Cannot parse condition: %1").arg(part).toStdString());
}

bool likeMatch(const QString &text, const QString &pattern)
{
    if (pattern == QLatin1String("%"))
        return true;
    const bool starts = pattern.startsWith(QLatin1Char('%'));
    const bool ends = pattern.endsWith(QLatin1Char('%'));
    const QString inner = QString(pattern).remove(QLatin1Char('%'));
    if (starts && ends)
        return text.contains(inner);
    if (starts)
        return text.endsWith(inner);
    if (ends)
        return text.startsWith(inner);
    return text == inner;
}

bool eval(const TableData &table, const Row &row, const Condition &cond)
{
    const QString cell = cond.column < row.values.size() ? row.values[cond.column] : QString();
    if (table.columns[cond.column].kind == ColumnKind::Number) {
        bool okLeft = false;
        bool okRight = false;
        const double left = cell.trimmed().toDouble(&okLeft);
        const double right = cond.value.trimmed().toDouble(&okRight);
        if (!okLeft || !okRight)
            return false;
        switch (cond.op) {
        case Op::Eq:
            return left == right;
        case Op::Ne:
            return left != right;
        case Op::Gt:
            return left > right;
        case Op::Ge:
            return left >= right;
        case Op::Lt:
            return left < right;
        case Op::Le:
            return left <= right;
        case Op::Like:
            return cell.contains(QString(cond.value).remove(QLatin1Char('%')));
        }
    }

    switch (cond.op) {
    case Op::Eq:
        return cell == cond.value;
    case Op::Ne:
        return cell != cond.value;
    case Op::Gt:
        return cell > cond.value;
    case Op::Ge:
        return cell >= cond.value;
    case Op::Lt:
        return cell < cond.value;
    case Op::Le:
        return cell <= cond.value;
    case Op::Like:
        return likeMatch(cell, cond.value);
    }
    return false;
}

} // namespace

QVector<int> filterRows(const TableData &table, const QString &expression)
{
    if (expression.trimmed().isEmpty()) {
        QVector<int> all;
        for (int i = 0; i < table.rows.size(); ++i)
            all.append(i);
        return all;
    }

    QList<QList<Condition>> groups;
    for (const QString &orPart : splitKeyword(expression, QStringLiteral("OR"))) {
        QList<Condition> group;
        for (const QString &andPart : splitKeyword(orPart, QStringLiteral("AND")))
            group.append(parseCondition(table, andPart));
        groups.append(group);
    }

    QVector<int> visible;
    for (int r = 0; r < table.rows.size(); ++r) {
        bool matchedAny = false;
        for (const QList<Condition> &group : groups) {
            bool matchedAll = true;
            for (const Condition &cond : group) {
                if (!eval(table, table.rows[r], cond)) {
                    matchedAll = false;
                    break;
                }
            }
            if (matchedAll) {
                matchedAny = true;
                break;
            }
        }
        if (matchedAny)
            visible.append(r);
    }
    return visible;
}
