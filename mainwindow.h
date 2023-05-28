#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "audiorecorder.h"

#include "zipfile.h"

#include <QMainWindow>
#include <QListWidget>
#include <QTreeWidget>
#include <QDir>
#include <QtSql>
#include <QMediaPlayer>
#include <QMap>
#include <QTimer>
#include <QElapsedTimer>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    enum RecordingType {
        RecNone = 0,
        RecNormal = 1,
        RecWatermark = 2
    };

    static void loadTranslation();

    static void setMainWnd(MainWindow *wnd);

    explicit MainWindow(QWidget *parent = 0, bool tempDB = false);
    ~MainWindow();

protected:
    virtual bool eventFilter(QObject *obj, QEvent *event);
    virtual void keyPressEvent(QKeyEvent *e);

private:
    static MainWindow* mainWnd;

    Ui::MainWindow *ui;

    QDir databaseDir;
    QSqlDatabase db;
    bool tempDatabase = false;
    ZipFile container;
    QMap<int, QString> containerAudio;
    int containerDuplicates = 0;
    QStringList presetNames;

    QSqlTableModel* recPresetsModel = Q_NULLPTR;
    AudioRecorder* recRecorder = Q_NULLPTR;
    QMediaPlayer* recPlayer = Q_NULLPTR;
    bool recPlayerNewMedia = false;
    QString recPlayerNewMediaFile;
    bool recRecorded = false;
    bool recAudioUpdated = false;
    QTimer* recUpdateTimer = Q_NULLPTR;

    QMediaPlayer* selPlayer = Q_NULLPTR;
    QTreeWidgetItem* selPlayItem = Q_NULLPTR;
    QElapsedTimer selPlayTimer;
    bool selSort = false;
    bool selSortDesc = false;
    QTimer* selUpdateTimer = Q_NULLPTR;

    QSqlTableModel* prefModelsModel = Q_NULLPTR;
    QSqlTableModel* prefBanksModel = Q_NULLPTR;
    QSqlTableModel* prefPresetsModel = Q_NULLPTR;
    QSqlTableModel* prefGroupsModel = Q_NULLPTR;

    QIcon blankIcon;
    QIcon playBtnIcon;
    QIcon stopBtnIcon;

    void addListItem(QListWidget* list, const QString& text, const QVariant& userValue) const;
    QString getListSelectionText(QListWidget* list) const;
    QString getListMaskText(QListWidget *list, int mask) const;
    int getListSelectionMask(QListWidget* list) const;
    void setListSelectionMask(QListWidget* list, int mask) const;
    void setInstruments(QListWidget* list) const;
    void setCategories(QListWidget* list) const;
    void setCharacters(QListWidget* list) const;

    void loadSettings();
    void initDatabase(bool tempDB);

    void showSqlError(const QSqlError& error, const QString& text = QString());
    bool execSqlQuery(QSqlQuery& query);
    bool execSqlQuery(const QString& query);
    bool execSqlQueryMulti(QSqlQuery& query, const QString& valueName, const QVariantList& valueList);

    QSortFilterProxyModel* createSortFilter(QAbstractItemModel* source, int sortColumn);

    QVariant getCurrentItem(const QListView* view, int column) const;
    bool setCurrentItem(QListView* view, int column, const QVariant& value) const;
    void addItem(QListView* view, int nameColumn, const QString& name, int idColumn = -1, const QVariant& id = QVariant(), int posColumn = -1);
    void removeCurrentItem(QListView* view, QSqlTableModel* sqlTableModel);

    void updateComboBox(QComboBox* comboBox, QSqlQuery& query, const QString& firstItem);

    QVariantList getCheckedItemIDs(QTreeWidget* tree);

    QString getPersistentMP3FilePath(const QVariant& id);
    QString getMP3FilePath(const QVariant& id);
    void deleteMP3Files();
    void deleteTempFiles();

    bool openContainer(const QString& name);
    QString duplicatesGroupName();
    bool updateDuplicatesGroup();

    void updatePresetNames(QComboBox* presetName);
    QString sqlTextSearch(const QString& fieldName, const QString& text);

    bool selIsSearchGlobal() const;

private slots:
    void tabChanged();

    void recUpdate();
    void recUpdateBanks();
    void recUpdatePresets();
    void recPresetNameChanged();
    void recSearchGlobalChanged();
    void recPresetsDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight);
    void recUpdatePresetData();
    void recAddPreset();
    void recSavePresetData();
    void recDeleteAudio();
    void recRecord();
    void recPlay();
    void recStop();
    void recSetPosition();
    void recMediaNotify();

    void selUpdate();
    void selUpdateBanks();
    void selUpdatePresets();
    void selPresetNameChanged();
    void selSearchGlobalChanged();
    void selPresetClicked(QTreeWidgetItem* item, int column);
    void selPresetCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
    void selSelectAll();
    void selDeselectAll();
    void selMultiSelectChanged();
    void selSortClicked();
    void selSortReset();
    void selSortUpdate();
    void selExportSelection();
    void selResGroupAdd();
    void selResGroupRemove();
    void selResDeleteAudio();
    void selResDeletePresets();
    void selStop();
    void selPlayerNotify();

    void eximExportContainer();
    void eximExportMP3();
    void eximOpenContainer();
    void eximImportContainer();
    void eximCloseContainer();

    void prefLanguageChanged();
    void prefChangeDatabaseDir();
    void prefAudioInputChanged();
    void prefRecordAudioOutputChanged();
    void prefMP3BitrateChanged();

    void prefUpdateModels();
    void prefAddModel();
    void prefRemoveModel();

    void prefUpdateBanks();
    void prefAddBank();
    void prefRemoveBank();
    void prefEditBanks();

    void prefUpdatePresets();
    void prefAddPreset();
    void prefRemovePreset();
    void prefEditPresets();
    void prefImportPresets();

    void prefUpdateGroups();
    void prefAddGroup();
    void prefRemoveGroup();
    void prefRemoveGroupPresets();

    void prefModelsDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight);
    void prefBanksDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight);
    void prefPresetsDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight);
    void prefGroupsDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight);

    void aboutUrlHandler(const QUrl& url);
};

#endif // MAINWINDOW_H
