#pragma once

#include "TableModel.h"

#include <QMainWindow>

class QEvent;
class QMenu;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QTableView;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(const QString &initialFile = QString(), QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void createUi();
    void createActions();
    void openFileDialog();
    void openPath(const QString &path);
    void reloadCurrent();
    void saveFile();
    void exportCsv();
    void applyFilter();
    void clearFilter();
    void fitContent();
    void fitWindow();
    void toggleDeleteMark();
    void updateCurrentCell(const QModelIndex &current = QModelIndex());
    void updateStatus();
    void updateCsvControls();
    QChar currentDelimiter() const;
    bool maybeSave();
    void showCellOverlay(const QModelIndex &index);
    void hideCellOverlay();
    void addRecentFile(const QString &path);
    void updateRecentMenu();

    TableModel *m_model = nullptr;
    QTableView *m_view = nullptr;
    QComboBox *m_encodingCombo = nullptr;
    QComboBox *m_delimiterCombo = nullptr;
    QCheckBox *m_headerCheck = nullptr;
    QLineEdit *m_filterEdit = nullptr;
    QLabel *m_countLabel = nullptr;
    QLabel *m_encodingLabel = nullptr;
    QLabel *m_cellLabel = nullptr;
    QLabel *m_pathLabel = nullptr;
    QLabel *m_cellOverlay = nullptr;
    QMenu *m_recentMenu = nullptr;
};
