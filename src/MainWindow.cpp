#include "MainWindow.h"

#include "Codec.h"
#include "CsvReader.h"
#include "DbfReader.h"
#include "Filter.h"
#include "XlsxReader.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFrame>
#include <QHeaderView>
#include <QHelpEvent>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QSettings>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QToolBar>
#include <stdexcept>

namespace {

class CellDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        const QString text = opt.text;
        opt.text.clear();

        QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

        if (text.isEmpty())
            return;

        QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, opt.widget);
        textRect.adjust(4, 0, -4, 0);

        const QString elided = opt.fontMetrics.elidedText(text, Qt::ElideRight, textRect.width());

        painter->save();
        painter->setPen(opt.palette.color(
            opt.state & QStyle::State_Selected ? QPalette::HighlightedText : QPalette::Text));
        painter->setFont(opt.font);
        painter->drawText(textRect, opt.displayAlignment, elided);
        painter->restore();
    }
};

FileFormat formatForPath(const QString &path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == QLatin1String("csv"))
        return FileFormat::Csv;
    if (ext == QLatin1String("xlsx"))
        return FileFormat::Xlsx;
    return FileFormat::Dbf;
}

TextEncoding comboEncoding(QComboBox *combo)
{
    return combo->currentText() == QLatin1String("UTF-8") ? TextEncoding::Utf8 : TextEncoding::Gbk;
}

QFrame *statusSeparator(QWidget *parent)
{
    auto *line = new QFrame(parent);
    line->setFrameShape(QFrame::VLine);
    line->setFrameShadow(QFrame::Sunken);
    line->setFixedHeight(16);
    return line;
}

} // namespace

MainWindow::MainWindow(const QString &initialFile, QWidget *parent)
    : QMainWindow(parent)
{
    createUi();
    createActions();
    if (!initialFile.isEmpty() && QFileInfo::exists(initialFile))
        openPath(initialFile);
}

void MainWindow::createUi()
{
    resize(1100, 700);
    setMinimumSize(760, 460);
    setWindowTitle(QStringLiteral("tableview"));
    setWindowIcon(QIcon(QStringLiteral(":/app.ico")));

    m_model = new TableModel(this);
    m_view = new QTableView(this);
    m_view->setModel(m_model);
    m_view->setItemDelegate(new CellDelegate(m_view));
    connect(m_view->selectionModel(), &QItemSelectionModel::currentChanged, this, &MainWindow::updateCurrentCell);
    m_view->setAlternatingRowColors(true);
    m_view->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_view->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
    m_view->horizontalHeader()->setSectionsMovable(true);
    m_view->horizontalHeader()->setSectionsClickable(true);
    m_view->setSortingEnabled(true);
    m_view->horizontalHeader()->setSortIndicatorClearable(true);
    m_view->verticalHeader()->setDefaultSectionSize(24);
    m_view->viewport()->installEventFilter(this);
    m_view->viewport()->setMouseTracking(true);

    m_cellOverlay = new QLabel(m_view->viewport());
    m_cellOverlay->hide();
    m_cellOverlay->setAutoFillBackground(true);
    m_cellOverlay->setFrameShape(QFrame::Box);
    m_cellOverlay->setContentsMargins(4, 0, 4, 0);
    m_cellOverlay->setAttribute(Qt::WA_TransparentForMouseEvents);

    setCentralWidget(m_view);

    auto *toolbar = addToolBar(QStringLiteral("Main"));
    toolbar->setMovable(false);

    toolbar->addAction(QStringLiteral("Open"), this, &MainWindow::openFileDialog)->setShortcut(QKeySequence::Open);
    toolbar->addAction(QStringLiteral("Save"), this, &MainWindow::saveFile)->setShortcut(QKeySequence::Save);
    toolbar->addAction(QStringLiteral("Export CSV"), this, &MainWindow::exportCsv);
    toolbar->addSeparator();

    toolbar->addWidget(new QLabel(QStringLiteral("Encoding")));
    m_encodingCombo = new QComboBox(this);
    m_encodingCombo->addItems({QStringLiteral("GBK"), QStringLiteral("UTF-8")});
    toolbar->addWidget(m_encodingCombo);
    connect(m_encodingCombo, &QComboBox::currentTextChanged, this, &MainWindow::reloadCurrent);

    toolbar->addSeparator();
    toolbar->addWidget(new QLabel(QStringLiteral("Delimiter")));
    m_delimiterCombo = new QComboBox(this);
    m_delimiterCombo->addItems({QStringLiteral(","), QStringLiteral(";"), QStringLiteral("Tab"), QStringLiteral("Space"), QStringLiteral("|")});
    toolbar->addWidget(m_delimiterCombo);
    connect(m_delimiterCombo, &QComboBox::currentTextChanged, this, &MainWindow::reloadCurrent);

    m_headerCheck = new QCheckBox(QStringLiteral("Header row"), this);
    m_headerCheck->setChecked(true);
    toolbar->addWidget(m_headerCheck);
    connect(m_headerCheck, &QCheckBox::toggled, this, &MainWindow::reloadCurrent);

    toolbar->addSeparator();
    toolbar->addWidget(new QLabel(QStringLiteral("Filter")));
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setMinimumWidth(240);
    toolbar->addWidget(m_filterEdit);
    connect(m_filterEdit, &QLineEdit::returnPressed, this, &MainWindow::applyFilter);
    toolbar->addAction(QStringLiteral("Apply"), this, &MainWindow::applyFilter);
    toolbar->addAction(QStringLiteral("Clear"), this, &MainWindow::clearFilter);
    toolbar->addSeparator();
    toolbar->addAction(QStringLiteral("Fit content"), this, &MainWindow::fitContent);
    toolbar->addAction(QStringLiteral("Fit window"), this, &MainWindow::fitWindow);

    m_countLabel = new QLabel(QStringLiteral("Ready"), this);
    m_encodingLabel = new QLabel(this);
    m_cellLabel = new QLabel(this);
    m_cellLabel->setMinimumWidth(180);
    m_pathLabel = new QLabel(this);
    m_pathLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    statusBar()->addWidget(m_countLabel);
    statusBar()->addWidget(statusSeparator(this));
    statusBar()->addWidget(m_encodingLabel);
    statusBar()->addWidget(statusSeparator(this));
    statusBar()->addWidget(m_cellLabel, 1);
    statusBar()->addPermanentWidget(statusSeparator(this));
    statusBar()->addPermanentWidget(m_pathLabel);

    updateCsvControls();
}

void MainWindow::createActions()
{
    auto *fileMenu = menuBar()->addMenu(QStringLiteral("File"));
    fileMenu->addAction(QStringLiteral("Open..."), QKeySequence::Open, this, &MainWindow::openFileDialog);
    fileMenu->addAction(QStringLiteral("Save"), QKeySequence::Save, this, &MainWindow::saveFile);
    fileMenu->addAction(QStringLiteral("Export CSV..."), this, &MainWindow::exportCsv);
    m_recentMenu = fileMenu->addMenu(QStringLiteral("Recent Files"));
    updateRecentMenu();
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("Exit"), this, &QWidget::close);

    auto *viewMenu = menuBar()->addMenu(QStringLiteral("View"));
    viewMenu->addAction(QStringLiteral("Fit content"), this, &MainWindow::fitContent);
    viewMenu->addAction(QStringLiteral("Fit window"), this, &MainWindow::fitWindow);

    auto *focusFilter = new QAction(this);
    focusFilter->setShortcut(QKeySequence::Find);
    addAction(focusFilter);
    connect(focusFilter, &QAction::triggered, m_filterEdit, qOverload<>(&QWidget::setFocus));

    auto *deleteMark = new QAction(this);
    deleteMark->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Delete));
    addAction(deleteMark);
    connect(deleteMark, &QAction::triggered, this, &MainWindow::toggleDeleteMark);
}

void MainWindow::openFileDialog()
{
    if (!maybeSave())
        return;
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Open file"), QString(), QStringLiteral("Data files (*.dbf *.csv *.xlsx);;All files (*.*)"));
    if (!path.isEmpty())
        openPath(path);
}

void MainWindow::openPath(const QString &path)
{
    try {
        const FileFormat format = formatForPath(path);
        TextEncoding encoding = TextEncoding::Utf8;
        if (format == FileFormat::Dbf)
            encoding = detectDbfEncoding(path);
        else if (format == FileFormat::Csv) {
            QFile file(path);
            file.open(QIODevice::ReadOnly);
            encoding = detectEncoding(file.read(4096));
        }
        {
            const QSignalBlocker blocker(m_encodingCombo);
            m_encodingCombo->setCurrentText(encodingLabel(encoding));
        }

        TableData table;
        if (format == FileFormat::Dbf)
            table = readDbfFile(path, encoding);
        else if (format == FileFormat::Csv)
            table = readCsvFile(path, encoding, currentDelimiter(), m_headerCheck->isChecked());
        else
            table = readXlsxFile(path);

        m_model->setTable(std::move(table));
        clearFilter();
        m_view->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
        fitContent();
        updateCsvControls();
        updateStatus();
        updateCurrentCell();
        setWindowTitle(QStringLiteral("tableview - %1").arg(QFileInfo(path).fileName()));
        addRecentFile(path);
    } catch (const std::exception &ex) {
        QMessageBox::warning(this, QStringLiteral("Open failed"), QString::fromUtf8(ex.what()));
    }
}

void MainWindow::reloadCurrent()
{
    if (!m_model->hasTable())
        return;
    const QString path = m_model->table().path;
    if (path.isEmpty())
        return;
    try {
        const FileFormat format = formatForPath(path);
        if (format == FileFormat::Xlsx) {
            statusBar()->showMessage(QStringLiteral("Excel files are UTF-8 internally; encoding selection does not apply."), 5000);
            return;
        }
        const TextEncoding encoding = comboEncoding(m_encodingCombo);
        TableData table;
        if (format == FileFormat::Dbf)
            table = readDbfFile(path, encoding);
        else if (format == FileFormat::Csv)
            table = readCsvFile(path, encoding, currentDelimiter(), m_headerCheck->isChecked());
        else
            table = readXlsxFile(path);
        m_model->setTable(std::move(table));
        clearFilter();
        fitContent();
        updateCsvControls();
        updateStatus();
        updateCurrentCell();
        statusBar()->showMessage(QStringLiteral("Reloaded with %1").arg(encodingLabel(encoding)), 3000);
    } catch (const std::exception &ex) {
        statusBar()->showMessage(QString::fromUtf8(ex.what()), 5000);
    }
}

void MainWindow::saveFile()
{
    if (!m_model->hasTable() || !m_model->table().supportsSave())
        return;
    try {
        saveDbfFile(m_model->table().path, m_model->table());
        m_model->setModified(false);
        updateStatus();
    } catch (const std::exception &ex) {
        QMessageBox::warning(this, QStringLiteral("Save failed"), QString::fromUtf8(ex.what()));
    }
}

void MainWindow::exportCsv()
{
    if (!m_model->hasTable())
        return;
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Export CSV"), QFileInfo(m_model->table().path).completeBaseName() + QStringLiteral(".csv"), QStringLiteral("CSV files (*.csv)"));
    if (path.isEmpty())
        return;
    try {
        QVector<int> visible;
        for (int r = 0; r < m_model->rowCount(); ++r)
            visible.append(m_model->sourceRowForProxyRow(r));
        exportCsvFile(path, m_model->table(), visible, currentDelimiter());
    } catch (const std::exception &ex) {
        QMessageBox::warning(this, QStringLiteral("Export failed"), QString::fromUtf8(ex.what()));
    }
}

void MainWindow::applyFilter()
{
    if (!m_model->hasTable())
        return;
    try {
        m_model->setVisibleRows(filterRows(m_model->table(), m_filterEdit->text()));
        updateStatus();
    } catch (const std::exception &ex) {
        QMessageBox::warning(this, QStringLiteral("Filter error"), QString::fromUtf8(ex.what()));
    }
}

void MainWindow::clearFilter()
{
    m_filterEdit->clear();
    if (m_model->hasTable())
        m_model->clearVisibleRows();
    updateStatus();
}

void MainWindow::fitContent()
{
    auto *header = m_view->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Interactive);

    const QFontMetrics fm(m_view->font());
    const int colCount = m_model->columnCount();
    const int rowCount = m_model->rowCount();

    for (int c = 0; c < colCount; ++c) {
        int maxWidth = fm.horizontalAdvance(m_model->headerData(c, Qt::Horizontal).toString());
        for (int r = 0; r < rowCount; ++r) {
            const QString text = m_model->data(m_model->index(r, c), Qt::DisplayRole).toString();
            maxWidth = std::max(maxWidth, fm.horizontalAdvance(text));
        }
        header->resizeSection(c, std::clamp(maxWidth + 16, 40, 500));
    }
}

void MainWindow::fitWindow()
{
    auto *header = m_view->horizontalHeader();
    const int cols = m_model->columnCount();
    if (cols == 0)
        return;

    header->setSectionResizeMode(QHeaderView::Interactive);

    int totalOriginal = 0;
    QVector<int> widths(cols);
    for (int i = 0; i < cols; ++i) {
        widths[i] = header->sectionSize(i);
        totalOriginal += widths[i];
    }
    if (totalOriginal == 0)
        return;

    const int available = m_view->viewport()->width();
    const double ratio = static_cast<double>(available) / totalOriginal;
    for (int i = 0; i < cols; ++i)
        header->resizeSection(i, std::clamp(static_cast<int>(std::round(widths[i] * ratio)), 20, 800));
}

void MainWindow::toggleDeleteMark()
{
    const QModelIndex index = m_view->currentIndex();
    if (!index.isValid())
        return;
    m_model->toggleDeleted(m_model->sourceRowForProxyRow(index.row()));
    updateStatus();
}

void MainWindow::updateCurrentCell(const QModelIndex &current)
{
    QModelIndex index = current.isValid() ? current : m_view->currentIndex();
    if (!index.isValid() || !m_model->hasTable()) {
        m_cellLabel->clear();
        return;
    }
    const QString value = m_model->data(index, Qt::DisplayRole).toString();
    const QString column = m_model->headerData(index.column(), Qt::Horizontal, Qt::DisplayRole).toString();
    m_cellLabel->setText(QStringLiteral("%1[%2]: %3").arg(column).arg(index.row() + 1).arg(value));
    m_cellLabel->setToolTip(value);
}

void MainWindow::updateStatus()
{
    if (!m_model->hasTable()) {
        m_countLabel->setText(QStringLiteral("Ready"));
        m_encodingLabel->clear();
        m_cellLabel->clear();
        m_pathLabel->clear();
        return;
    }
    const int total = m_model->table().rows.size();
    const int visible = m_model->rowCount();
    m_countLabel->setText(visible == total ? QStringLiteral("Rows: %1").arg(total) : QStringLiteral("Rows: %1, filtered: %2").arg(total).arg(visible));
    m_encodingLabel->setText(QStringLiteral("%1 / %2%3")
                                 .arg(formatLabel(m_model->table().format), encodingLabel(m_model->table().encoding),
                                      m_model->isModified() ? QStringLiteral(" / Modified") : QString()));
    m_pathLabel->setText(m_model->table().path);
    m_pathLabel->setToolTip(m_model->table().path);
    m_pathLabel->adjustSize();
}

void MainWindow::updateCsvControls()
{
    const bool csv = m_model->hasTable() && m_model->table().format == FileFormat::Csv;
    m_delimiterCombo->setVisible(csv);
    m_headerCheck->setVisible(csv);
}

QChar MainWindow::currentDelimiter() const
{
    const QString text = m_delimiterCombo->currentText();
    if (text == QLatin1String("Tab"))
        return QLatin1Char('\t');
    if (text == QLatin1String("Space"))
        return QLatin1Char(' ');
    return text.isEmpty() ? QLatin1Char(',') : text[0];
}

bool MainWindow::maybeSave()
{
    if (!m_model->hasTable() || !m_model->isModified() || !m_model->table().supportsSave())
        return true;
    const QMessageBox::StandardButton result = QMessageBox::question(this, QStringLiteral("tableview"), QStringLiteral("The DBF file has unsaved changes. Save now?"), QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    if (result == QMessageBox::Cancel)
        return false;
    if (result == QMessageBox::Yes)
        saveFile();
    return true;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (maybeSave())
        event->accept();
    else
        event->ignore();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_view->viewport()) {
        if (event->type() == QEvent::ToolTip) {
            auto *help = static_cast<QHelpEvent *>(event);
            showCellOverlay(m_view->indexAt(help->pos()));
            return true;
        }
        if (event->type() == QEvent::MouseMove
            || event->type() == QEvent::Leave
            || event->type() == QEvent::MouseButtonPress
            || event->type() == QEvent::Wheel) {
            hideCellOverlay();
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::showCellOverlay(const QModelIndex &index)
{
    if (!index.isValid()) {
        hideCellOverlay();
        return;
    }

    const QString value = m_model->data(index, Qt::DisplayRole).toString();
    if (value.isEmpty()) {
        hideCellOverlay();
        return;
    }

    const QRect rect = m_view->visualRect(index);
    const QFontMetrics fm(m_view->font());
    const int textWidth = fm.horizontalAdvance(value);
    if (textWidth <= rect.width() - 8) {
        hideCellOverlay();
        return;
    }

    m_cellOverlay->setText(value);
    m_cellOverlay->setFont(m_view->font());
    m_cellOverlay->setGeometry(rect.x(), rect.y(), textWidth + 12, rect.height());
    m_cellOverlay->show();
    m_cellOverlay->raise();
}

void MainWindow::hideCellOverlay()
{
    m_cellOverlay->hide();
}

void MainWindow::addRecentFile(const QString &path)
{
    const QString absolute = QFileInfo(path).absoluteFilePath();
    QSettings settings;
    QStringList recent = settings.value(QStringLiteral("recentFiles")).toStringList();
    recent.removeAll(absolute);
    recent.prepend(absolute);
    while (recent.size() > 10)
        recent.removeLast();
    settings.setValue(QStringLiteral("recentFiles"), recent);
    updateRecentMenu();
}

void MainWindow::updateRecentMenu()
{
    m_recentMenu->clear();
    const QStringList recent = QSettings().value(QStringLiteral("recentFiles")).toStringList();
    if (recent.isEmpty()) {
        m_recentMenu->addAction(QStringLiteral("(empty)"))->setEnabled(false);
        return;
    }
    for (const QString &filePath : recent) {
        auto *action = m_recentMenu->addAction(QFileInfo(filePath).fileName());
        action->setToolTip(filePath);
        connect(action, &QAction::triggered, this, [this, filePath]() {
            if (!maybeSave())
                return;
            if (QFileInfo::exists(filePath))
                openPath(filePath);
            else
                statusBar()->showMessage(QStringLiteral("File not found: %1").arg(filePath), 5000);
        });
    }
}
