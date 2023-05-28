#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "editordialog.h"
#include "recpresetsmodel.h"
#include "zipfile.h"
#include "licensedialog.h"
#include "logdialog.h"

#include <QSettings>
#include <QStandardPaths>
#include <QFileDialog>
#include <QMessageBox>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QTranslator>
#include <QDesktopServices>
#include <QKeyEvent>

//#define PATCHDIRECTOR_LITE

template<class Model> class AutoFetchAll : public Model
{
    using Model::Model;

    virtual void queryChange() override
    {
        while (this->canFetchMore(QModelIndex())) this->fetchMore(QModelIndex());
    }
};

static QString currentDirName;

static QString currentDir()
{
    if (currentDirName.isEmpty()) {
        currentDirName = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    }
    return currentDirName;
}

static void updateCurrentDir(const QString& file)
{
    if (!file.isEmpty()) currentDirName = QFileInfo(file).absolutePath();
}

static QString language;
static QTranslator qtTranslator;
static QTranslator appTranslator;

void MainWindow::loadTranslation()
{
    qApp->removeTranslator(&qtTranslator);
    qApp->removeTranslator(&appTranslator);
    QSettings settings;
    language = settings.value("Language").toString();
    if (language.isEmpty()) {
        language = QLocale::system().name();
        language.truncate(2);
    }
    if (language != "de") language = "en";
    if (language == "en") return;
    if (qtTranslator.load("qt_" + language, QLibraryInfo::location(QLibraryInfo::TranslationsPath))) {
        qApp->installTranslator(&qtTranslator);
    }
    if (appTranslator.load(":/translations/patchdirector_" + language)) {
        qApp->installTranslator(&appTranslator);
    }
}

void MainWindow::setMainWnd(MainWindow *wnd)
{
    if (mainWnd) {
        if (mainWnd->isVisible()) mainWnd->close();
        mainWnd->deleteLater();
    }
    mainWnd = wnd;
    if (!mainWnd->isVisible()) mainWnd->show();
}

MainWindow::MainWindow(QWidget *parent, bool tempDB) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    blankIcon("://res/blank-32.png"),
    playBtnIcon("://res/media-play-btn-32.png"),
    stopBtnIcon("://res/media-stop-btn-32.png")
{
    ui->setupUi(this);

#ifdef Q_OS_WIN
    resize(width(), height() + 40);

    QMargins margins = ui->groupBox_12->contentsMargins();
    margins.setBottom(9);
    ui->groupBox_12->setContentsMargins(margins);

    margins = ui->horizontalLayout_8->contentsMargins();
    margins.setTop(6);
    ui->horizontalLayout_8->setContentsMargins(margins);

    ui->selSelectAll->setMinimumWidth(72);
    ui->selDeselectAll->setMinimumWidth(72);
    ui->selSort->setMinimumWidth(40);

    ui->label_32->hide();
    ui->prefRecordAudioOutput->hide();
#endif

#ifdef PATCHDIRECTOR_LITE
    setWindowTitle("Patchdirector Lite");
    ui->selExportSelection->setEnabled(false);
    ui->eximExportGroupBox->setEnabled(false);
#else
    setWindowTitle("Patchdirector Pro");
#endif

    ui->recPresetName->lineEdit()->setPlaceholderText(tr("Search for preset name"));
    ui->selPresetName->lineEdit()->setPlaceholderText(tr("Search for preset name"));
    //ui->recPresetName->lineEdit()->setClearButtonEnabled(true);
    ui->selResGroup->lineEdit()->setPlaceholderText(tr("Select or create Group"));
    //ui->selPresetName->lineEdit()->setClearButtonEnabled(true);

    setInstruments(ui->recInstrument);
    setInstruments(ui->selInstrument);
    setCategories(ui->recCategory);
    setCategories(ui->selCategory);
    setCharacters(ui->recCharacter);
    setCharacters(ui->selCharacter);

    ui->infoTextBrowser->setSource(QUrl(tr("qrc:/res/Info.html")));

    loadSettings();
    deleteTempFiles();

    connect(ui->tabWidget, SIGNAL(currentChanged(int)), SLOT(tabChanged()));

    connect(ui->recModel, SIGNAL(currentIndexChanged(int)), SLOT(recUpdateBanks()));
    connect(ui->recBank, SIGNAL(currentIndexChanged(int)), SLOT(recUpdatePresets()));
    connect(ui->recGroup, SIGNAL(currentIndexChanged(int)), SLOT(recUpdatePresets()));
    connect(ui->recPresetName, SIGNAL(editTextChanged(QString)), SLOT(recPresetNameChanged()));
    ui->recPresetName->installEventFilter(this);
    connect(ui->recSearchGlobal, SIGNAL(stateChanged(int)), SLOT(recSearchGlobalChanged()));
    connect(ui->recAddPreset, SIGNAL(clicked(bool)), SLOT(recAddPreset()));
    connect(ui->recUndo, SIGNAL(clicked(bool)), SLOT(recUpdatePresetData()));
    connect(ui->recSaveNext, SIGNAL(clicked(bool)), SLOT(recSavePresetData()));
    connect(ui->recDeleteAudio, SIGNAL(clicked(bool)), SLOT(recDeleteAudio()));
    connect(ui->recRecord, SIGNAL(clicked(bool)), SLOT(recRecord()));
    connect(ui->recRecordWM, SIGNAL(clicked(bool)), SLOT(recRecord()));
    connect(ui->recPlay, SIGNAL(clicked(bool)), SLOT(recPlay()));
    connect(ui->recStop, SIGNAL(clicked(bool)), SLOT(recStop()));
    connect(ui->recSlider, SIGNAL(sliderMoved(int)), SLOT(recSetPosition()));
    connect(ui->recSlider, SIGNAL(sliderReleased()), SLOT(recSetPosition()));
    recRecorder = new AudioRecorder(this);
    connect(recRecorder, SIGNAL(notify()), SLOT(recMediaNotify()));
    recPlayer = new QMediaPlayer(this);
    connect(recPlayer, SIGNAL(stateChanged(QMediaPlayer::State)), SLOT(recMediaNotify()));
    connect(recPlayer, SIGNAL(mediaStatusChanged(QMediaPlayer::MediaStatus)), SLOT(recMediaNotify()));
    connect(recPlayer, SIGNAL(durationChanged(qint64)), SLOT(recMediaNotify()));
    connect(recPlayer, SIGNAL(positionChanged(qint64)), SLOT(recMediaNotify()));
    recUpdateTimer = new QTimer(this);
    recUpdateTimer->setSingleShot(true);
    connect(recUpdateTimer, SIGNAL(timeout()), SLOT(recUpdatePresets()));

    connect(ui->selModel, SIGNAL(currentIndexChanged(int)), SLOT(selUpdateBanks()));
    connect(ui->selBank, SIGNAL(currentIndexChanged(int)), SLOT(selUpdatePresets()));
    connect(ui->selGroup, SIGNAL(currentIndexChanged(int)), SLOT(selUpdatePresets()));
    connect(ui->selPresetName, SIGNAL(editTextChanged(QString)), SLOT(selPresetNameChanged()));
    ui->selPresetName->installEventFilter(this);
    connect(ui->selSearchGlobal, SIGNAL(stateChanged(int)), SLOT(selSearchGlobalChanged()));
    connect(ui->selInstrument, SIGNAL(itemSelectionChanged()), SLOT(selUpdatePresets()));
    connect(ui->selCategory, SIGNAL(itemSelectionChanged()), SLOT(selUpdatePresets()));
    connect(ui->selCharacter, SIGNAL(itemSelectionChanged()), SLOT(selUpdatePresets()));
    connect(ui->selPresetsWithoutAudio, SIGNAL(stateChanged(int)), SLOT(selUpdatePresets()));
    ui->selPresets->installEventFilter(this);
    connect(ui->selPresets, SIGNAL(itemClicked(QTreeWidgetItem*,int)), SLOT(selPresetClicked(QTreeWidgetItem*,int)));
    connect(ui->selPresets, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)), SLOT(selPresetCurrentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)));
    connect(ui->selSelectAll, SIGNAL(clicked(bool)), SLOT(selSelectAll()));
    connect(ui->selDeselectAll, SIGNAL(clicked(bool)), SLOT(selDeselectAll()));
    connect(ui->selMultiSelect, SIGNAL(stateChanged(int)), SLOT(selMultiSelectChanged()));
    connect(ui->selSort, SIGNAL(clicked(bool)), SLOT(selSortClicked()));
    connect(ui->selExportSelection, SIGNAL(clicked(bool)), SLOT(selExportSelection()));
    connect(ui->selResGroupAdd, SIGNAL(clicked(bool)), SLOT(selResGroupAdd()));
    connect(ui->selResGroupRemove, SIGNAL(clicked(bool)), SLOT(selResGroupRemove()));
    connect(ui->selResDeleteAudio, SIGNAL(clicked(bool)), SLOT(selResDeleteAudio()));
    connect(ui->selResDeletePresets, SIGNAL(clicked(bool)), SLOT(selResDeletePresets()));
    selPlayer = new QMediaPlayer(this);
    connect(selPlayer, SIGNAL(stateChanged(QMediaPlayer::State)), SLOT(selPlayerNotify()));
    selUpdateTimer = new QTimer(this);
    selUpdateTimer->setSingleShot(true);
    connect(selUpdateTimer, SIGNAL(timeout()), SLOT(selUpdatePresets()));

    connect(ui->eximExportContainer, SIGNAL(clicked(bool)), SLOT(eximExportContainer()));
    connect(ui->eximExportMP3, SIGNAL(clicked(bool)), SLOT(eximExportMP3()));
    connect(ui->eximOpenContainer, SIGNAL(clicked(bool)), SLOT(eximOpenContainer()));
    connect(ui->eximImportContainer, SIGNAL(clicked(bool)), SLOT(eximImportContainer()));
    connect(ui->eximCloseContainer, SIGNAL(clicked(bool)), SLOT(eximCloseContainer()));

    connect(ui->prefLanguage, SIGNAL(currentIndexChanged(int)), SLOT(prefLanguageChanged()));
    connect(ui->prefChangeDBDir, SIGNAL(clicked(bool)), SLOT(prefChangeDatabaseDir()));
    connect(ui->prefAudioInput, SIGNAL(currentIndexChanged(int)), SLOT(prefAudioInputChanged()));
    connect(ui->prefRecordAudioOutput, SIGNAL(currentIndexChanged(int)), SLOT(prefRecordAudioOutputChanged()));
    connect(ui->prefMP3Bitrate128, SIGNAL(toggled(bool)), SLOT(prefMP3BitrateChanged()));
    connect(ui->prefMP3Bitrate160, SIGNAL(toggled(bool)), SLOT(prefMP3BitrateChanged()));

    connect(ui->prefAddModel, SIGNAL(clicked(bool)), SLOT(prefAddModel()));
    connect(ui->prefRemoveModel, SIGNAL(clicked(bool)), SLOT(prefRemoveModel()));

    connect(ui->prefAddBank, SIGNAL(clicked(bool)), SLOT(prefAddBank()));
    connect(ui->prefRemoveBank, SIGNAL(clicked(bool)), SLOT(prefRemoveBank()));
    connect(ui->prefEditBanks, SIGNAL(clicked(bool)), SLOT(prefEditBanks()));

    connect(ui->prefAddPreset, SIGNAL(clicked(bool)), SLOT(prefAddPreset()));
    connect(ui->prefRemovePreset, SIGNAL(clicked(bool)), SLOT(prefRemovePreset()));
    connect(ui->prefEditPresets, SIGNAL(clicked(bool)), SLOT(prefEditPresets()));
    connect(ui->prefImportPresets, SIGNAL(clicked(bool)), SLOT(prefImportPresets()));

    connect(ui->prefAddGroup, SIGNAL(clicked(bool)), SLOT(prefAddGroup()));
    connect(ui->prefRemoveGroup, SIGNAL(clicked(bool)), SLOT(prefRemoveGroup()));
    connect(ui->prefRemoveGroupPresets, SIGNAL(clicked(bool)), SLOT(prefRemoveGroupPresets()));

    QDesktopServices::setUrlHandler("about", this, "aboutUrlHandler");

    initDatabase(tempDB);

    ui->tabWidget->setCurrentWidget(ui->tabRecord);
    tabChanged();
}

MainWindow::~MainWindow()
{
    delete ui;
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->selPresets) {
        if (event->type() == QEvent::KeyPress) {
             QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
             if (keyEvent->key() == Qt::Key_Space) {
                 QTreeWidgetItem* item = ui->selPresets->currentItem();
                 if (item) {
                     item->setCheckState(0,  (item->checkState(0) == Qt::Checked) ? Qt::Unchecked : Qt::Checked);
                     selPresetClicked(item, 0);
                     return true;
                 }
             }
        }
    } else if (obj == ui->recPresetName || obj == ui->selPresetName) {
        QComboBox* presetName = qobject_cast<QComboBox*>(obj);
        if (presetName && event->type() == QEvent::FocusOut) {
            QString name = presetName->currentText();
            if (!name.isEmpty()) {
                int i = presetName->findText(name, Qt::MatchFixedString);
                if (i >= 0) presetName->removeItem(i);
                presetName->insertItem(0, name);
                presetName->removeItem(15);
                presetNames.clear();
                for (i = 0; i < presetName->count(); ++i) presetNames.append(presetName->itemText(i));
                QSettings settings;
                settings.setValue("PresetNames", presetNames);
                presetName->setCurrentIndex(0);
                QAbstractItemView* view = presetName->view();
                view->setCurrentIndex(view->model()->index(0, view->currentIndex().column()));
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_L && e->modifiers() == (Qt::ShiftModifier | Qt::ControlModifier)) {
        LogDialog::create();
    }
}

MainWindow* MainWindow::mainWnd = Q_NULLPTR;

void MainWindow::addListItem(QListWidget *list, const QString &text, const QVariant &userValue) const
{
    QListWidgetItem* item = new QListWidgetItem(text);
    item->setData(Qt::UserRole, userValue);
    list->addItem(item);
}

QString MainWindow::getListSelectionText(QListWidget *list) const
{
    QString text;
    foreach (QListWidgetItem* item, list->selectedItems()) {
        if (!text.isEmpty()) text += ", ";
        text += item->text();
    }
    return text;
}

QString MainWindow::getListMaskText(QListWidget *list, int mask) const
{
    QString text;
    for (int i = 0; i < list->count(); ++i) {
        QListWidgetItem* item = list->item(i);
        int id = item->data(Qt::UserRole).toInt();
        if (id > 0 && mask & (1 << (id - 1))) {
            if (!text.isEmpty()) text += ", ";
            text += item->text();
        }
    }
    return text;
}

int MainWindow::getListSelectionMask(QListWidget *list) const
{
    int mask = 0;
    foreach (QListWidgetItem* item, list->selectedItems()) {
        int id = item->data(Qt::UserRole).toInt();
        if (id > 0) mask |= 1 << (id - 1);
    }
    return mask;
}

void MainWindow::setListSelectionMask(QListWidget *list, int mask) const
{
    QSignalBlocker signalBlocker(list);
    list->clearSelection();
    for (int i = 0; i < list->count(); ++i) {
        int id = list->item(i)->data(Qt::UserRole).toInt();
        if (id > 0 && mask & (1 << (id - 1))) list->item(i)->setSelected(true);
    }
}

void MainWindow::setInstruments(QListWidget *list) const
{
    addListItem(list, tr("Bass"),               1);
    addListItem(list, tr("Bell"),               2);
    addListItem(list, tr("Brass"),              3);
    addListItem(list, tr("Ethnic"),             4);
    addListItem(list, tr("Drums/Percussion"),   21);
    addListItem(list, tr("Guitar"),             5);
    addListItem(list, tr("Keyboard"),           6);
    addListItem(list, tr("Lead"),               7);
    addListItem(list, tr("Orchestral"),         8);
    addListItem(list, tr("Choir"),              9);
    addListItem(list, tr("Organ"),              10);
    addListItem(list, tr("Woodwind"),           20);
    addListItem(list, tr("Pad"),                11);
    addListItem(list, tr("Piano"),              12);
    addListItem(list, tr("Rhythmic"),           13);
    addListItem(list, tr("Strings"),            14);
    addListItem(list, tr("Voice"),              15);
    addListItem(list, tr("SFX"),                16);
    addListItem(list, tr("Ambient/Soundscape"), 17);
    addListItem(list, tr("Synth"),              18);
    addListItem(list, tr("1-Key-Chord"),        19);
}

void MainWindow::setCategories(QListWidget *list) const
{
    addListItem(list, tr("Acoustic"),               1);
    addListItem(list, tr("Electronic/Electric"),    2);
    addListItem(list, tr("Blended Instruments"),    3);
    addListItem(list, tr("Aftertouch"),             18);
    addListItem(list, tr("Decrease/Fall"),          4);
    addListItem(list, tr("Evolving/Rise"),          5);
    addListItem(list, tr("Short Attack/Hit"),       6);
    addListItem(list, tr("Ensemble"),               7);
    addListItem(list, tr("Wood"),                   8);
    addListItem(list, tr("Metal"),                  9);
    addListItem(list, tr("Glass"),                  10);
    addListItem(list, tr("Mono"),                   11);
    addListItem(list, tr("Arpeggio/Sequence"),      12);
    addListItem(list, tr("Sample/Loop"),            13);
    addListItem(list, tr("Stacked/Multi"),          14);
    addListItem(list, tr("Wind/Noise"),             15);
    addListItem(list, tr("Soundtrack"),             16);
    addListItem(list, tr("Plucked"),                17);
    addListItem(list, tr("Bowed"),                  19);
    addListItem(list, tr("Reverse"),                20);
}

void MainWindow::setCharacters(QListWidget *list) const
{
    addListItem(list, tr("Glitter/Shimmering"),       1);
    addListItem(list, tr("Sub-Bass"),                 2);
    addListItem(list, tr("Bright"),                   3);
    addListItem(list, tr("Dark"),                     4);
    addListItem(list, tr("Clean"),                    5);
    addListItem(list, tr("Distorted/Dirty"),          6);
    addListItem(list, tr("Complexity - Low"),         7);
    addListItem(list, tr("Complexity - Medium/High"), 8);
    addListItem(list, tr("Hard"),                     9);
    addListItem(list, tr("Soft"),                     10);
    addListItem(list, tr("Thin"),                     11);
    addListItem(list, tr("Fat"),                      12);
    addListItem(list, tr("Growl/Formant"),            13);
    addListItem(list, tr("Delay/PingPong"),           14);
    addListItem(list, tr("Pulsating/Gate"),           15);
    addListItem(list, tr("Random/Chaos"),             16);
    addListItem(list, tr("Floating"),                 17);
    addListItem(list, tr("Sweep"),                    18);
    addListItem(list, tr("Room - Small/Medium"),      19);
    addListItem(list, tr("Room - Big"),               20);
    addListItem(list, tr("Phasing"),                  21);
    addListItem(list, tr("Atonal"),                   22);
    addListItem(list, tr("EDM"),                      23);
}

void MainWindow::loadSettings()
{
    QSettings settings;

    QSignalBlocker(ui->prefLanguage);
    ui->prefLanguage->addItem("English", "en");
    ui->prefLanguage->addItem("Deutsch", "de");
    int i = ui->prefLanguage->findData(language);
    if (i < 0) i = 0;
    ui->prefLanguage->setCurrentIndex(i);

    QString dir = settings.value("DatabaseDirectory").toString();
    if (dir.isEmpty()) {
        QDir::home().mkdir(QCoreApplication::applicationName());
        dir = QDir::home().filePath(QCoreApplication::applicationName());
    }
    databaseDir.setPath(dir);
    ui->prefDBDir->setText(databaseDir.path());

#ifdef Q_OS_WIN
    ui->prefAudioInput->addItem(tr("<Default>") + " (Loopback)");
#else
    ui->prefAudioInput->addItem(tr("<Default>"));
#endif
    ui->prefAudioInput->addItems(AudioRecorder::inputDeviceNames());
    ui->prefAudioInput->setCurrentText(AudioRecorder::inputDeviceName());
    if (ui->prefAudioInput->currentIndex() < 0) ui->prefAudioInput->setCurrentIndex(0);

#ifdef Q_OS_OSX
    ui->prefRecordAudioOutput->addItem(tr("<Default>"));
    ui->prefRecordAudioOutput->addItems(AudioRecorder::systemOutputDeviceNames());
    ui->prefRecordAudioOutput->setCurrentText(AudioRecorder::defaultOutputDeviceName());
    if (ui->prefRecordAudioOutput->currentIndex() < 0) ui->prefRecordAudioOutput->setCurrentIndex(0);
#endif

    switch (AudioRecorder::MP3Bitrate()) {
        case 128: ui->prefMP3Bitrate128->setChecked(true); break;
        case 160: ui->prefMP3Bitrate160->setChecked(true); break;
    }

    presetNames = settings.value("PresetNames").toStringList();
    while (presetNames.size() < 15) presetNames.append("");
}

void MainWindow::initDatabase(bool tempDB)
{
    tempDatabase = false;
    QString dbName = databaseDir.absoluteFilePath("Patchdirector.db");
    db = QSqlDatabase::addDatabase("QSQLITE");
    if (!tempDB) {
        db.setDatabaseName(dbName);
        if (!db.open()) showSqlError(db.lastError(), tr("The database could not be opened."));
    }
    if (!db.isOpen()) {
        db.setDatabaseName("");
        if (!db.open()) showSqlError(db.lastError());
        tempDatabase = true;
        if (tempDB) {
            QSqlQuery query(db);
            query.prepare("ATTACH DATABASE ? AS db");
            query.addBindValue(dbName);
            execSqlQuery(query);
        }
    }
    execSqlQuery("PRAGMA foreign_keys = ON");
    execSqlQuery(
        "CREATE TABLE IF NOT EXISTS models ("
        "    id INTEGER PRIMARY KEY,"
        "    name TEXT COLLATE NOCASE NOT NULL CHECK(name<>''),"
        "    UNIQUE (name)"
        ")");
    execSqlQuery(
        "CREATE TABLE IF NOT EXISTS banks ("
        "    id INTEGER PRIMARY KEY,"
        "    model INTEGER NOT NULL REFERENCES models ON UPDATE CASCADE ON DELETE CASCADE,"
        "    name TEXT COLLATE NOCASE NOT NULL CHECK(name<>''),"
        "    UNIQUE (model, name)"
        ")");
    execSqlQuery(
        "CREATE TABLE IF NOT EXISTS presets ("
        "    id INTEGER PRIMARY KEY,"
        "    bank INTEGER NOT NULL REFERENCES banks ON UPDATE CASCADE ON DELETE CASCADE,"
        "    name TEXT COLLATE NOCASE NOT NULL CHECK(name<>''),"
        "    position INTEGER,"
        "    instrument INTEGER,"
        "    category INTEGER,"
        "    character INTEGER,"
        "    recorded INTEGER,"
        "    UNIQUE (bank, name)"
        ")");
    execSqlQuery(
        "CREATE TABLE IF NOT EXISTS groups ("
        "    id INTEGER PRIMARY KEY,"
        "    name TEXT COLLATE NOCASE NOT NULL CHECK(name<>''),"
        "    UNIQUE (name)"
        ")");
    execSqlQuery(
        "CREATE TABLE IF NOT EXISTS presetgroups ("
        "    preset INTEGER NOT NULL REFERENCES presets ON UPDATE CASCADE ON DELETE CASCADE,"
        "    groupid INTEGER NOT NULL REFERENCES groups ON UPDATE CASCADE ON DELETE CASCADE,"
        "    UNIQUE (preset, groupid)"
        ")");
    execSqlQuery(
        "CREATE TEMP TABLE deletedpresets ("
        "    id INTEGER NOT NULL,"
        "    UNIQUE (id)"
        ")");
    execSqlQuery(
        "CREATE TEMP TRIGGER deletepreset AFTER DELETE ON main.presets BEGIN"
        "    INSERT OR IGNORE INTO deletedpresets (id) VALUES (old.id);"
        "END");
}

void MainWindow::showSqlError(const QSqlError& error, const QString& text)
{
    QString s(text);
    if (s.isEmpty()) s = tr("A database error occured.");
    if (error.isValid()) {
        //if (!error.driverText().isEmpty()) s += '\n' + error.driverText();
        if (!error.databaseText().isEmpty()) s += '\n' + error.databaseText();
    }
    QMessageBox::critical(this, QCoreApplication::applicationName(), s);
}

bool MainWindow::execSqlQuery(QSqlQuery& query)
{
    if (query.exec()) return true;
    showSqlError(query.lastError());
    return false;
}

bool MainWindow::execSqlQuery(const QString &query)
{
    QSqlQuery q(db);
    if (q.exec(query)) return true;
    showSqlError(q.lastError());
    return false;
}

bool MainWindow::execSqlQueryMulti(QSqlQuery &query, const QString &valueName, const QVariantList &valueList)
{
    if (!db.transaction()) {
        showSqlError(db.lastError());
        return false;
    }
    bool ok = true;
    foreach (QVariant value, valueList) {
        query.bindValue(valueName, value);
        if (!(ok = execSqlQuery(query))) break;
    }
    if (ok) {
        ok = db.commit();
        if (!ok) showSqlError(db.lastError());
    }
    if (!ok) db.rollback();
    return ok;
}

QSortFilterProxyModel *MainWindow::createSortFilter(QAbstractItemModel *source, int sortColumn)
{
    QSortFilterProxyModel* sortFilter = new QSortFilterProxyModel(this);
    sortFilter->setSourceModel(source);
    sortFilter->setSortCaseSensitivity(Qt::CaseInsensitive);
    sortFilter->setSortLocaleAware(true);
    sortFilter->setDynamicSortFilter(false);
    sortFilter->sort(sortColumn);
    return sortFilter;
}

QVariant MainWindow::getCurrentItem(const QListView* view, int column) const
{
    QAbstractItemModel* model = view->model();
    QModelIndex idx = view->currentIndex();
    if (!model || !idx.isValid()) return QVariant();
    return model->data(model->index(idx.row(), column));
}

bool MainWindow::setCurrentItem(QListView* view, int column, const QVariant &value) const
{
    QAbstractItemModel* model = view->model();
    if (!model) return false;
    for (int i = 0; i < model->rowCount(); ++i) {
        if (model->data(model->index(i, column)) == value) {
            view->setCurrentIndex(model->index(i, view->modelColumn()));
            return true;
        }
    }
    return false;
}

void MainWindow::addItem(QListView *view, int nameColumn, const QString &name, int idColumn, const QVariant& id, int posColumn)
{
    QAbstractItemModel* model = view->model();
    if (!model || idColumn >= 0 && !id.isValid()) return;
    int row = model->rowCount();
    model->insertRow(row);
    if (idColumn >= 0) model->setData(model->index(row, idColumn), id);
    if (posColumn >= 0) {
        QVariant pos;
        if (row > 0) pos = model->data(model->index(row - 1, posColumn));
        if (!pos.isValid()) pos = 0;
        pos = pos.toInt() + 1;
        model->setData(model->index(row, posColumn), pos);
    }
    model->setData(model->index(row, nameColumn), name);
    setCurrentItem(view, nameColumn, name);
    if (view->editTriggers() != QAbstractItemView::NoEditTriggers) view->edit(view->currentIndex());
}

void MainWindow::removeCurrentItem(QListView *view, QSqlTableModel *sqlTableModel)
{
    QAbstractItemModel* model = view->model();
    QModelIndex index = view->currentIndex();
    if (!model || !index.isValid()) return;
    if (QMessageBox::question(this, QCoreApplication::applicationName(),
        tr("Do you really want to delete \"%1\"?").arg(model->data(index).toString()))
        != QMessageBox::Yes) return;
    model->removeRow(index.row());
    sqlTableModel->submitAll();
    sqlTableModel->select();
}

void MainWindow::updateComboBox(QComboBox *comboBox, QSqlQuery &query, const QString &firstItem)
{
    QSignalBlocker signalBlocker(comboBox);
    QString currentText = comboBox->currentText();
    QVariant currentID = comboBox->currentData();
    comboBox->clear();
    if (!firstItem.isEmpty()) comboBox->addItem(firstItem);
    AutoFetchAll<QSqlQueryModel> model;
    model.setQuery(query);
    QSortFilterProxyModel* sortFilter = createSortFilter(&model, 1);
    for (int i = 0; i < sortFilter->rowCount(); ++i) {
        comboBox->addItem(sortFilter->data(sortFilter->index(i, 1)).toString(), sortFilter->data(sortFilter->index(i, 0)));
    }
    delete sortFilter;
    if (comboBox->isEditable()) {
        comboBox->setCurrentIndex(-1);
        comboBox->setCurrentText(currentText);
    } else {
        int idx = comboBox->findData(currentID);
        if (idx < 0 && !firstItem.isEmpty()) idx = 0;
        if (idx >= 0) comboBox->setCurrentIndex(idx);
    }
}

QVariantList MainWindow::getCheckedItemIDs(QTreeWidget *tree)
{
    QVariantList ids;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = ui->selPresets->topLevelItem(i);
        if (item->checkState(0) == Qt::Checked) {
            QVariant id = item->data(0, Qt::UserRole);
            if (id.isValid()) ids.append(id);
        }
    }
    return ids;
}

QString MainWindow::getPersistentMP3FilePath(const QVariant &id)
{
    int i = id.toInt();
    QString dir = QString::number(i / 1000);
    databaseDir.mkdir(dir);
    return databaseDir.absoluteFilePath(dir + "/" + QString::number(i) + ".mp3");
}

QString MainWindow::getMP3FilePath(const QVariant &id)
{
    if (!tempDatabase) return getPersistentMP3FilePath(id);
    int i = id.toInt();
    databaseDir.mkdir("temp");
    QString path = databaseDir.absoluteFilePath(
        "temp/" + QString::number(QCoreApplication::applicationPid()) + "-" + QString::number(i) + ".mp3");
    if (container.isOpen() && !QFile(path).exists() && containerAudio.contains(i)) {
        if (!container.extractFile(containerAudio[i], path)) {
            QMessageBox::critical(this, QCoreApplication::applicationName(),
                tr("The audio file could not be extracted from the container."));
        }
    }
    return path;
}

void MainWindow::deleteMP3Files()
{
    QSqlQuery query(db);
    query.prepare("SELECT id FROM deletedpresets");
    if (!execSqlQuery(query)) return;
    QDir dir;
    while (query.next()) {
        QVariant id = query.value(0);
        QString name = getMP3FilePath(id);
        if (id.isValid() && !name.isEmpty()) dir.remove(name);
    }
    query.prepare("DELETE FROM deletedpresets");
    execSqlQuery(query);
}

void MainWindow::deleteTempFiles()
{
    QDir dir(databaseDir.absoluteFilePath("temp"));
    dir.setFilter(QDir::Files);
    foreach(QString dirFile, dir.entryList()) dir.remove(dirFile);
}

bool MainWindow::openContainer(const QString &name)
{
    if (!tempDatabase) return false;
    containerAudio.clear();
    deleteTempFiles();
    if (!container.open(name)) return false;
    QByteArray xmlBuffer;
    if (!container.extractFile("Patchdirector.xml", xmlBuffer)) return false;
    QXmlStreamReader xml(xmlBuffer);
    if (!xml.readNextStartElement() || xml.name() != "patchdirector") return false;
    QSqlQuery query(db);
    while (xml.readNextStartElement()) {
        if (xml.name() == "model") {
            query.prepare("INSERT INTO models (name) VALUES (:name)");
            query.bindValue(":name", xml.attributes().value("name").toString());
            if (!execSqlQuery(query)) return false;
            QVariant model = query.lastInsertId();
            while (xml.readNextStartElement()) {
                if (xml.name() == "bank") {
                    query.prepare("INSERT INTO banks (model, name) VALUES (:model, :name)");
                    query.bindValue(":model", model);
                    query.bindValue(":name", xml.attributes().value("name").toString());
                    if (!execSqlQuery(query)) return false;
                    QVariant bank = query.lastInsertId();
                    int position = 1;
                    while (xml.readNextStartElement()) {
                        if (xml.name() == "preset") {
                            query.prepare(
                                "INSERT INTO presets (bank, name, position, instrument, category, character, recorded) "
                                "VALUES (:bank, :name, :position, :instrument, :category, :character, :recorded)");
                            query.bindValue(":bank", bank);
                            query.bindValue(":name", xml.attributes().value("name").toString());
                            query.bindValue(":position", position++);
                            query.bindValue(":instrument", xml.attributes().value("instrument").toInt());
                            query.bindValue(":category", xml.attributes().value("category").toInt());
                            query.bindValue(":character", xml.attributes().value("character").toInt());
                            QString audio = xml.attributes().value("audio").toString();
                            int rec = RecNone;
                            if (!audio.isEmpty()) {
                                rec = xml.attributes().value("watermark").toInt() ? RecWatermark : RecNormal;
                            }
                            query.bindValue(":recorded", rec);
                            if (!execSqlQuery(query)) return false;
                            QVariant preset = query.lastInsertId();
                            if (!audio.isEmpty()) containerAudio[preset.toInt()] = audio;
                            while (xml.readNextStartElement()) {
                                if (xml.name() == "group") {
                                    QString group = xml.attributes().value("name").toString();
                                    query.prepare("INSERT OR IGNORE INTO groups (name) VALUES (:name)");
                                    query.bindValue(":name", group);
                                    if (!execSqlQuery(query)) return false;
                                    query.prepare(
                                       "INSERT INTO presetgroups (preset, groupid) "
                                       "VALUES (:preset, (SELECT id FROM groups WHERE name=:group))");
                                    query.bindValue(":preset", preset);
                                    query.bindValue(":group", group);
                                    if (!execSqlQuery(query)) return false;
                                }
                                xml.skipCurrentElement();
                            }
                        } else {
                            xml.skipCurrentElement();
                        }
                    }
                } else {
                    xml.skipCurrentElement();
                }
            }
        } else {
            xml.skipCurrentElement();
        }
    }
    updateDuplicatesGroup();
    return true;
}

QString MainWindow::duplicatesGroupName()
{
    return tr("<Duplicates>");
}

bool MainWindow::updateDuplicatesGroup()
{
    containerDuplicates = 0;
    if (!container.isOpen()) return false;
    QString name = duplicatesGroupName();
    QSqlQuery query(db);
    query.prepare("INSERT OR IGNORE INTO groups (name) VALUES (?)");
    query.addBindValue(name);
    if (!execSqlQuery(query)) return false;
    query.prepare("DELETE FROM presetgroups WHERE groupid=(SELECT id FROM groups WHERE name=?)");
    query.addBindValue(name);
    if (!execSqlQuery(query)) return false;
    query.prepare(
        "INSERT INTO presetgroups (preset, groupid) "
        "SELECT p1.id, groups.id "
        "FROM presets AS p1, models AS m1, banks AS b1, db.presets AS p2, db.models AS m2, db.banks AS b2, groups "
        "WHERE b1.id=p1.bank AND m1.id=b1.model AND b2.id=p2.bank AND m2.id=b2.model AND "
        "m1.name=m2.name AND b1.name=b2.name AND p1.name=p2.name AND p1.recorded AND p2.recorded AND groups.name=?");
    query.addBindValue(name);
    if (!execSqlQuery(query)) return false;
    containerDuplicates = query.numRowsAffected();
    return true;
}

void MainWindow::updatePresetNames(QComboBox *presetName)
{
    QSignalBlocker signalBlocker(presetName);
    QString text = presetName->currentText();
    presetName->clear();
    presetName->addItems(presetNames);
    presetName->setCurrentIndex(-1);
    presetName->setCurrentText(text);
}

QString MainWindow::sqlTextSearch(const QString &fieldName, const QString &text)
{
    QString s = text;
    s.replace('@', "@@");
    s.replace('_', "@_");
    s.replace('%', "@%");
    QSqlField field;
    field.setType(QVariant::String);
    field.setValue("%" + s + "%");
    return fieldName + " LIKE " + db.driver()->formatValue(field) + " ESCAPE '@'";
}

bool MainWindow::selIsSearchGlobal() const
{
    return ui->selModel->currentIndex() <= 0 && ui->selGroup->currentIndex() <= 0 &&
        ui->selInstrument->selectedItems().isEmpty() && ui->selCategory->selectedItems().isEmpty() &&
        ui->selCharacter->selectedItems().isEmpty();
}

void MainWindow::tabChanged()
{
    QWidget* current = ui->tabWidget->currentWidget();
    if (current == ui->tabRecord) {
        updateDuplicatesGroup();
        updatePresetNames(ui->recPresetName);
        recUpdate();
        recMediaNotify();
    } else if (current == ui->tabSelect) {
        updateDuplicatesGroup();
        updatePresetNames(ui->selPresetName);
        selUpdate();
    } else if (current == ui->tabPreferences) {
        prefUpdateModels();
        prefUpdateGroups();
    }
}

void MainWindow::recUpdate()
{
    QSqlQuery query(db);
    query.prepare("SELECT id, name FROM models");
    query.exec();
    updateComboBox(ui->recModel, query, tr("<Select Model>"));
    query.prepare("SELECT id, name FROM groups");
    query.exec();
    updateComboBox(ui->recGroup, query, tr("<All Groups>"));
    recUpdateBanks();
}

void MainWindow::recUpdateBanks()
{
    QVariant id = ui->recModel->currentData();
    if (!id.isValid()) id = -1;
    if (id >= 0) {
        ui->recGroup->setCurrentIndex(0);
        ui->recSearchGlobal->setChecked(false);
    }
    QSqlQuery query(db);
    query.prepare("SELECT id, name FROM banks WHERE model=?");
    query.addBindValue(id);
    query.exec();
    updateComboBox(ui->recBank, query, tr("<Select Bank>"));
    recUpdatePresets();
}

void MainWindow::recUpdatePresets()
{
    if (!recPresetsModel) {
        recPresetsModel = new AutoFetchAll<RecPresetsModel>(this, db);
        recPresetsModel->setTable("presets");
        recPresetsModel->setSort(3, Qt::AscendingOrder);
        recPresetsModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
        connect(recPresetsModel, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)), SLOT(recPresetsDataChanged(QModelIndex,QModelIndex)));
        ui->recPresets->setModel(recPresetsModel);
        ui->recPresets->setModelColumn(2);
        connect(ui->recPresets->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)), SLOT(recUpdatePresetData()));
    }
    QVariant id = getCurrentItem(ui->recPresets, 0);
    QString filter;
    QString name = ui->recPresetName->currentText();
    QVariant group = ui->recGroup->currentData();
    if (group.isValid()) {
        filter = "id IN (SELECT preset FROM presetgroups WHERE groupid=" + group.toString() + ")";
        ui->recModel->setCurrentIndex(0);
        ui->recSearchGlobal->setChecked(false);
    } else {
        QVariant bank = ui->recBank->currentData();
        if (bank.isValid() || name.isEmpty()) {
            if (!bank.isValid()) bank = -1;
            filter = "bank=" + bank.toString();
        }
    }
    if (!name.isEmpty())  {
        if (!filter.isEmpty()) filter += " AND ";
        filter += sqlTextSearch("name", name);
    }
    recPresetsModel->setFilter(filter);
    recPresetsModel->select();
    ui->recPresetCount->setText("(" + QString::number(recPresetsModel->rowCount()) + ")");
    setCurrentItem(ui->recPresets, 0, id);
    recUpdatePresetData();
}

void MainWindow::recPresetNameChanged()
{
    if (!ui->recPresetName->currentText().isEmpty()) {
        if (ui->recBank->currentIndex() <= 0 && ui->recGroup->currentIndex() <= 0) {
            ui->recSearchGlobal->setChecked(true);
        }
    } else {
        ui->recSearchGlobal->setChecked(false);
    }
    recUpdateTimer->start(100);
}

void MainWindow::recSearchGlobalChanged()
{
    if (ui->recSearchGlobal->isChecked()) {
        ui->recModel->setCurrentIndex(0);
        ui->recGroup->setCurrentIndex(0);
    } else {
        ui->recPresetName->clearEditText();
    }
}

void MainWindow::recPresetsDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    if (topLeft.column() == 1 || topLeft.column() >= 3) return;
    QVariant id = getCurrentItem(ui->recPresets, 0);
    recPresetsModel->submitAll();
    recPresetsModel->select();
    if (id.isValid()) setCurrentItem(ui->recPresets, 0, id);
    ui->recPresets->setFocus();
}

void MainWindow::recUpdatePresetData()
{
    QVariant id;
    int instrument = 0;
    int category = 0;
    int character = 0;
    recRecorded = false;
    QModelIndex index = ui->recPresets->currentIndex();
    if (index.isValid()) {
        int row = index.row();
        QAbstractItemModel* model = ui->recPresets->model();
        id = model->data(model->index(row, 0)).toString();
        instrument = model->data(model->index(row, 4)).toInt();
        category = model->data(model->index(row, 5)).toInt();
        character = model->data(model->index(row, 6)).toInt();
        recRecorded = model->data(model->index(row, 7)).toInt() != RecNone;
    }
    setListSelectionMask(ui->recInstrument, instrument);
    setListSelectionMask(ui->recCategory, category);
    setListSelectionMask(ui->recCharacter, character);
    recStop();
    recRecorder->clear();
    recPlayerNewMediaFile.clear();
    if (id.isValid() && recRecorded) {
        recPlayerNewMediaFile = getMP3FilePath(id);
    }
    recPlayerNewMedia = true;
    recAudioUpdated = false;
    recMediaNotify();
}

void MainWindow::recAddPreset()
{
    if (ui->recGroup->currentData().isValid()) return;
    QVariant id = ui->recBank->currentData();
    if (id.isValid()) addItem(ui->recPresets, 2, tr("New Preset"), 1, id, 3);
}

void MainWindow::recSavePresetData()
{
    recStop();
    QModelIndex index = ui->recPresets->currentIndex();
    if (!index.isValid()) return;
    if (!getListSelectionMask(ui->recInstrument) &&
        (recAudioUpdated && recRecorder->maxPosition() > 0 || !recAudioUpdated && recRecorded))
    {
        QMessageBox::information(this, QCoreApplication::applicationName(),
            tr("Please select an instrument before saving."));
        return;
    }
    int row = index.row();
    QAbstractItemModel* model = ui->recPresets->model();
    QVariant id = model->data(model->index(row, 0)).toString();
    model->setData(model->index(row, 4), getListSelectionMask(ui->recInstrument));
    model->setData(model->index(row, 5), getListSelectionMask(ui->recCategory));
    model->setData(model->index(row, 6), getListSelectionMask(ui->recCharacter));
    if (recAudioUpdated) {
        if (recRecorder->maxPosition() > 0) {
            if (!recRecorder->saveMP3(getMP3FilePath(id))) {
                QMessageBox::critical(this, QCoreApplication::applicationName(),
                    tr("An error occured while saving recording as MP3."));
                recRecorder->clear();
            }
        }
        int rec = RecNone;
        if (recRecorder->maxPosition() > 0) rec = recRecorder->hasWatermark() ? RecWatermark : RecNormal;
        model->setData(model->index(row, 7), rec);
    }
    if (!recPresetsModel->submitAll()) {
        showSqlError(recPresetsModel->lastError());
    }
    setCurrentItem(ui->recPresets, 0, id);
    recUpdatePresets();
    index = ui->recPresets->currentIndex();
    if (index.row() < ui->recPresets->model()->rowCount() - 1) {
        ui->recPresets->setCurrentIndex(ui->recPresets->model()->index(index.row() + 1, index.column()));
    }
}

void MainWindow::recDeleteAudio()
{
    if (!getCurrentItem(ui->recPresets, 0).isValid()) return;
    recStop();
    recRecorder->clear();
    recAudioUpdated = true;
    recMediaNotify();
}

void MainWindow::recRecord()
{
    if (!getCurrentItem(ui->recPresets, 0).isValid()) {
        ui->recRecord->setChecked(false);
        ui->recRecordWM->setChecked(false);
        return;
    }
    bool watermark = sender() == ui->recRecordWM;
    if (recRecorder->state() != AudioRecorder::Recording || watermark != recRecorder->hasWatermark()) {
        recAudioUpdated = true;
        recPlayer->stop();
        recPlayer->setMedia(QMediaContent());
        recRecorder->record(watermark);
    } else {
        recRecorder->stop();
    }
    recMediaNotify();
}

void MainWindow::recPlay()
{
    if (recAudioUpdated) {
        if (recRecorder->state() != AudioRecorder::Playing) {
            recRecorder->play();
        } else {
            recRecorder->stop();
        }
    } else if (recRecorded) {
        if (recPlayer->state() == QMediaPlayer::PlayingState) {
            recPlayer->pause();
        } else {
            recPlayer->setNotifyInterval(100);
            recPlayer->play();
        }
    }
    recMediaNotify();
}

void MainWindow::recStop()
{
    recRecorder->stop();
    recRecorder->setPosition(0);
    recPlayer->stop();
    recPlayer->setPosition(0);
    recMediaNotify();
}

void MainWindow::recSetPosition()
{
    if (recAudioUpdated) {
        int maxPos = recRecorder->maxPosition();
        if (maxPos > 0) recRecorder->setPosition(ui->recSlider->value() * maxPos / 1000);
        recMediaNotify();
    } else {
        int duration = recPlayer->duration();
        if (duration > 0) recPlayer->setPosition(ui->recSlider->value() * duration / 1000);
    }
}

static QString posToTime(int pos)
{
    pos /= 1000;
    return QString::asprintf("%02d:%02d", pos / 60, pos % 60);
}

void MainWindow::recMediaNotify()
{
    if (recPlayerNewMedia) {
        //qDebug() << recPlayer->mediaStatus();
        if (recPlayer->mediaStatus() != QMediaPlayer::LoadingMedia) {
            recPlayerNewMedia = false;
            if (recPlayerNewMediaFile.isEmpty()) {
                recPlayer->setMedia(QMediaContent());
            } else {
                recPlayer->setMedia(QUrl::fromLocalFile(recPlayerNewMediaFile));
            }
        }
    }
    if (recAudioUpdated) {
        bool play = false;
        bool rec = false;
        bool recWM = false;
        switch (recRecorder->state()) {
            case AudioRecorder::Recording:
                recWM = recRecorder->hasWatermark();
                rec = !recWM;
                break;
            case AudioRecorder::Playing:
                play = true;
                break;
        }
        if (ui->recPlay->isChecked() != play) ui->recPlay->setChecked(play);
        if (ui->recRecord->isChecked() != rec) ui->recRecord->setChecked(rec);
        if (ui->recRecordWM->isChecked() != recWM) ui->recRecordWM->setChecked(recWM);
        int pos = recRecorder->position();
        int maxPos = recRecorder->maxPosition();
        if (!ui->recSlider->isSliderDown()) ui->recSlider->setValue(maxPos > 0 ? pos * 1000 / maxPos : 0);
        ui->recTime->setText(posToTime(pos) + " / " + posToTime(maxPos));
    } else {
        if (recPlayer->state() == QMediaPlayer::PlayingState) {
            if (!ui->recPlay->isChecked()) ui->recPlay->setChecked(true);
        } else {
            if (ui->recPlay->isChecked()) {
                ui->recPlay->setChecked(false);
#ifdef Q_OS_WIN
                if (recPlayer->mediaStatus() == QMediaPlayer::EndOfMedia) {
                    QMediaContent media = recPlayer->media();
                    recPlayer->setMedia(media);
                }
#endif
            }
        }
        int pos = recPlayer->position();
        int duration = recPlayer->duration();
        if (!ui->recSlider->isSliderDown()) ui->recSlider->setValue(duration > 0 ? pos * 1000 / duration : 0);
        ui->recTime->setText(posToTime(pos) + " / " + posToTime(duration));
        //qDebug() << recPlayer->state() << " " << recPlayer->mediaStatus();
    }
}

void MainWindow::selUpdate()
{
    QSqlQuery query(db);
    query.prepare("SELECT id, name FROM models");
    query.exec();
    updateComboBox(ui->selModel, query, tr("<All Models>"));
    query.prepare("SELECT id, name FROM groups");
    query.exec();
    updateComboBox(ui->selGroup, query, tr("<All Groups>"));
    query.prepare("SELECT id, name FROM groups WHERE name<>?");
    query.addBindValue(duplicatesGroupName());
    query.exec();
    updateComboBox(ui->selResGroup, query, "");
    selUpdateBanks();
}

void MainWindow::selUpdateBanks()
{
    QVariant id = ui->selModel->currentData();
    if (!id.isValid()) id = -1;
    if (id >= 0) {
        ui->selSearchGlobal->setChecked(false);
    }
    QSqlQuery query(db);
    query.prepare("SELECT id, name FROM banks WHERE model=?");
    query.addBindValue(id);
    query.exec();
    updateComboBox(ui->selBank, query, tr("<All Banks>"));
    selUpdatePresets();
}

void MainWindow::selUpdatePresets()
{
    //static int ctr = 0; qDebug() << "selUpdatePresets" << ++ctr;
    selStop();
    if (!selIsSearchGlobal()) ui->selSearchGlobal->setChecked(false);
    QVariantList checkedIDs = getCheckedItemIDs(ui->selPresets);
    QSet<qlonglong> checkedIDSet;
    foreach (QVariant id, checkedIDs) checkedIDSet.insert(id.toLongLong());
    ui->selPresets->clear();
    QString from = "presets, models, banks";
    QString where = "banks.id=presets.bank AND models.id=banks.model";
    QVariant id = ui->selBank->currentData();
    if (id.isValid()) {
        where += " AND presets.bank=" + id.toString();
    } else {
        id = ui->selModel->currentData();
        if (id.isValid()) where += " AND banks.model=" + id.toString();
    }
    id = ui->selGroup->currentData();
    if (id.isValid()) {
        from += ", presetgroups";
        where += " AND presetgroups.preset=presets.id AND presetgroups.groupid=" + id.toString();
    }
    int mask = getListSelectionMask(ui->selInstrument);
    if (mask) where += QString(" AND (presets.instrument & %1)").arg(mask);
    mask = getListSelectionMask(ui->selCategory);
    if (mask) where += QString(" AND (presets.category & %1)=%1").arg(mask);
    mask = getListSelectionMask(ui->selCharacter);
    if (mask) where += QString(" AND (presets.character & %1)=%1").arg(mask);
    if (!ui->selPresetsWithoutAudio->isChecked()) where += " AND presets.recorded";
    QString name = ui->selPresetName->currentText();
    if (!name.isEmpty()) where += " AND " + sqlTextSearch("presets.name", name);
    QString orderBy = "models.name, banks.name, presets.position";
    if (selSort) orderBy = QString("presets.name ") + (selSortDesc ? "DESC" : "ASC");
    QString queryString = "SELECT presets.id, presets.name, presets.recorded, models.name, banks.name FROM " + from +
            " WHERE " + where + " ORDER BY " + orderBy;
    //qDebug() << queryString;
    QSqlQuery query(db);
    query.prepare(queryString);
    if (!execSqlQuery(query)) return;
    QList<QTreeWidgetItem*> itemList;
    while (query.next()) {
        QTreeWidgetItem* item = new QTreeWidgetItem();
        item->setCheckState(0, Qt::Unchecked);
        QVariant id = query.value(0);
        item->setData(0, Qt::UserRole, id);
        if (id.isValid() && checkedIDSet.contains(id.toLongLong())) item->setCheckState(0, Qt::Checked);
        if (query.value(2).toInt()) {
            item->setIcon(1, playBtnIcon);
            item->setData(1, Qt::UserRole, true);
        } else {
            item->setIcon(1, blankIcon);
        }
        item->setText(2, query.value(1).toString());
        item->setToolTip(2, query.value(3).toString() + "\n" + query.value(4).toString());
        itemList.append(item);
    }
    ui->selPresets->addTopLevelItems(itemList);
    ui->selPresets->resizeColumnToContents(0);
    ui->selPresets->resizeColumnToContents(1);
    ui->selPresets->resizeColumnToContents(2);
    ui->selPresets->reset();
    //ui->selPresets->doItemsLayout();
    ui->selPresetCount->setText("(" + QString::number(itemList.size()) + ")");
}

void MainWindow::selPresetNameChanged()
{
    if (!ui->selPresetName->currentText().isEmpty()) {
        if (selIsSearchGlobal()) ui->selSearchGlobal->setChecked(true);
    } else {
        ui->selSearchGlobal->setChecked(false);
    }
    selUpdateTimer->start(100);
}

void MainWindow::selSearchGlobalChanged()
{
    if (ui->selSearchGlobal->isChecked()) {
        ui->selModel->setCurrentIndex(0);
        ui->selGroup->setCurrentIndex(0);
        ui->selInstrument->clearSelection();
        ui->selCategory->clearSelection();
        ui->selCharacter->clearSelection();
    } else {
        ui->selPresetName->clearEditText();
    }
}

void MainWindow::selPresetClicked(QTreeWidgetItem *item, int column)
{
    if (column == 0 && !ui->selMultiSelect->isChecked() && item->checkState(0) == Qt::Checked) {
        for (int i = 0; i < ui->selPresets->topLevelItemCount(); ++i) {
            QTreeWidgetItem* item2 = ui->selPresets->topLevelItem(i);
            if (item2 != item && item2->checkState(0) == Qt::Checked) item2->setCheckState(0, Qt::Unchecked);
        }
    }
    if (column == 1 && item->data(1, Qt::UserRole).toBool()) {
        if (item == selPlayItem) {
            if (selPlayTimer.elapsed() > 100) selStop();
        } else {
            selStop();
            item->setIcon(1, stopBtnIcon);
            selPlayItem = item;
            selPlayer->setMedia(QUrl::fromLocalFile(getMP3FilePath(item->data(0, Qt::UserRole))));
            selPlayer->play();
            selPlayTimer.start();
        }
    }
}

void MainWindow::selPresetCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    if (selPlayItem && current != selPlayItem) selPresetClicked(current, 1);
}

void MainWindow::selSelectAll()
{
    ui->selMultiSelect->setChecked(true);
    for (int i = 0; i < ui->selPresets->topLevelItemCount(); ++i) {
        ui->selPresets->topLevelItem(i)->setCheckState(0, Qt::Checked);
    }
}

void MainWindow::selDeselectAll()
{
    for (int i = 0; i < ui->selPresets->topLevelItemCount(); ++i) {
        ui->selPresets->topLevelItem(i)->setCheckState(0, Qt::Unchecked);
    }
}

void MainWindow::selMultiSelectChanged()
{
    if (!ui->selMultiSelect->isChecked()) selDeselectAll();
}

void MainWindow::selSortClicked()
{
    if (!selSort) {
        selSort = true;
        selSortDesc = false;
    } else if (!selSortDesc) {
        selSortDesc = true;
    } else {
        selSort = false;
        selSortDesc = false;
    }
    selSortUpdate();
}

void MainWindow::selSortReset()
{
    if (selSort) {
        selSort = false;
        selSortDesc = false;
        selSortUpdate();
    }
}

void MainWindow::selSortUpdate()
{
    ui->selSort->setChecked(selSort);
    ui->selSort->setText(selSortDesc ? "Z-A" : "A-Z");
    selUpdatePresets();
    ui->selPresets->scrollToTop();
}

void MainWindow::selExportSelection()
{
    ui->tabWidget->setCurrentWidget(ui->tabExImport);
}

void MainWindow::selResGroupAdd()
{
    QVariantList ids = getCheckedItemIDs(ui->selPresets);
    QString groupName = ui->selResGroup->currentText();
    if (ids.isEmpty() || groupName.isEmpty()) return;
    QSqlQuery query(db);
    query.prepare("INSERT OR IGNORE INTO groups (name) VALUES (:groupname)");
    query.bindValue(":groupname", groupName);
    if (!execSqlQuery(query)) return;
    query.prepare("SELECT id FROM groups WHERE name=:groupname");
    query.bindValue(":groupname", groupName);
    if (!execSqlQuery(query)) return;
    if (!query.first()) return;
    QVariant groupID = query.value(0);
    query.prepare("INSERT OR IGNORE INTO presetgroups (preset, groupid) VALUES (:preset, :groupid)");
    query.bindValue(":groupid", groupID);
    execSqlQueryMulti(query, ":preset", ids);
    selUpdate();
}

void MainWindow::selResGroupRemove()
{
    if (container.isOpen() && ui->selGroup->currentText() == duplicatesGroupName()) return;
    QVariantList ids = getCheckedItemIDs(ui->selPresets);
    QVariant groupID = ui->selGroup->currentData();
    if (ids.isEmpty() || !groupID.isValid()) return;
    if (QMessageBox::question(this, QCoreApplication::applicationName(),
        tr("Do you want to remove the selected preset(s) from group \"%1\"?").arg(ui->selGroup->currentText()))
        != QMessageBox::Yes) return;
    QSqlQuery query(db);
    query.prepare("DELETE FROM presetgroups WHERE preset=:preset AND groupid=:groupid");
    query.bindValue(":groupid", groupID);
    execSqlQueryMulti(query, ":preset", ids);
    selUpdatePresets();
}

void MainWindow::selResDeleteAudio()
{
    QVariantList ids = getCheckedItemIDs(ui->selPresets);
    if (ids.isEmpty()) return;
    if (QMessageBox::question(this, QCoreApplication::applicationName(),
        tr("Do you really want to delete the audio recordings of the selected presets?"))
        != QMessageBox::Yes) return;
    QSqlQuery query(db);
    query.prepare("UPDATE presets SET recorded=0 WHERE id=:id");
    execSqlQueryMulti(query, ":id", ids);
    selUpdatePresets();
}

void MainWindow::selResDeletePresets()
{
    QVariantList ids = getCheckedItemIDs(ui->selPresets);
    if (ids.isEmpty()) return;
    if (QMessageBox::question(this, QCoreApplication::applicationName(),
        tr("Do you really want to delete the selected presets?"))
        != QMessageBox::Yes) return;
    QSqlQuery query(db);
    query.prepare("DELETE FROM presets WHERE id=:id");
    execSqlQueryMulti(query, ":id", ids);
    selUpdatePresets();
    deleteMP3Files();
}

void MainWindow::selStop()
{
    if (!selPlayItem) return;
    selPlayItem->setIcon(1, playBtnIcon);
    selPlayItem = Q_NULLPTR;
    selPlayer->stop();
    selPlayer->setMedia(QMediaContent());
}

void MainWindow::selPlayerNotify()
{
    if (selPlayer->state() == QMediaPlayer::StoppedState) selStop();
}

void MainWindow::eximExportContainer()
{
#ifndef PATCHDIRECTOR_LITE
    selSortReset();
    QVariantList ids = getCheckedItemIDs(ui->selPresets);
    if (ids.isEmpty()) return;
    QString fileName = QFileDialog::getSaveFileName(
        this, tr("Select container file"), currentDir(), tr("Patchdirector containers (*.padi)"));
    if (fileName.isEmpty()) return;
    updateCurrentDir(fileName);
    ZipFile cont;
    if (!cont.create(fileName)) return;
    QByteArray xmlBuffer;
    QXmlStreamWriter xml(&xmlBuffer);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement("patchdirector");
    xml.writeAttribute("version", "1.0");
    QString currentModel, currentBank;
    int mp3 = 1;
    QSqlQuery query(db);
    query.prepare(
        "SELECT presets.name, presets.instrument, presets.category, presets.character, presets.recorded, models.name, banks.name "
        "FROM presets, models, banks "
        "WHERE presets.id=? AND banks.id=presets.bank AND models.id=banks.model");
    QSqlQuery groupQuery(db);
    groupQuery.prepare("SELECT groups.name FROM presetgroups, groups "
        "WHERE presetgroups.preset=? AND groups.id=presetgroups.groupid "
        "ORDER BY groups.name");
    foreach (const QVariant& id, ids) {
        query.bindValue(0, id);
        if (!execSqlQuery(query)) return;
        if (query.first()) {
            QString model = query.value(5).toString();
            QString bank = query.value(6).toString();
            if (model != currentModel || bank != currentBank) {
                if (!currentBank.isEmpty()) xml.writeEndElement();
                if (model != currentModel) {
                    if (!currentModel.isEmpty()) xml.writeEndElement();
                    xml.writeStartElement("model");
                    xml.writeAttribute("name", model);
                    currentModel = model;
                }
                xml.writeStartElement("bank");
                xml.writeAttribute("name", bank);
                currentBank = bank;
            }
            xml.writeStartElement("preset");
            xml.writeAttribute("name", query.value(0).toString());
            xml.writeAttribute("instrument", QString::number(query.value(1).toInt()));
            xml.writeAttribute("category", QString::number(query.value(2).toInt()));
            xml.writeAttribute("character", QString::number(query.value(3).toInt()));
            int recorded = query.value(4).toInt();
            if (recorded != RecNone) {
                QString mp3Name = QString::asprintf("%d.mp3", mp3++);
                if (cont.addFile(mp3Name, getMP3FilePath(id))) {
                    xml.writeAttribute("audio", mp3Name);
                }
                if (recorded == RecWatermark) xml.writeAttribute("watermark", "1");
            }
            groupQuery.bindValue(0, id);
            if (!execSqlQuery(groupQuery)) return;
            while (groupQuery.next()) {
                xml.writeEmptyElement("group");
                xml.writeAttribute("name", groupQuery.value(0).toString());
            }
            xml.writeEndElement();
        }
    }
    xml.writeEndDocument();
    cont.addFile("Patchdirector.xml", xmlBuffer);
    QScopedPointer<EditorDialog> editor(new EditorDialog(this));
    editor->setWindowTitle(tr("Enter Container Info (optional)"));
    if (editor->exec() == QDialog::Accepted) {
        QString text = editor->text();
        if (!text.isEmpty()) cont.addFile("Info.txt", text.toUtf8());
    }
#endif
}

static QString toValidFileName(const QString& name)
{
    QString invalid = "/\\?%*:|\"<>.~";
    QString s = name;
    for (int i = 0; i < s.size(); ++i) if (s[i] < 0x20 || invalid.contains(s[i])) s[i] = ' ';
    return s.simplified().trimmed();
}

static QString escapeCsvField(const QString& data)
{
    QString result = data;
    result.replace('"', "\"\"");
    return '"' + result + '"';
}

static void saveCsvFile(
    const QStringList& csvList, const QString& path, const QString& csvName, ZipFile& zip, const QDir& dir)
{
    if (csvList.isEmpty()) return;
    QString csvPath = path + "/" + csvName;
    QByteArray csvData = csvList.join("\r\n").toLatin1();
    if (zip.isOpen()) {
        zip.addFile(csvPath, csvData);
    } else {
        dir.mkpath(path);
        QFile file(dir.absoluteFilePath(csvPath));
        if (file.open(QIODevice::WriteOnly)) file.write(csvData);
    }
}

void MainWindow::eximExportMP3()
{
#ifndef PATCHDIRECTOR_LITE
    selSortReset();
    QVariantList ids = getCheckedItemIDs(ui->selPresets);
    if (ids.isEmpty()) return;
    ZipFile zip;
    QString dirPath;
    if (ui->eximMP3Zip->isChecked()) {
        QString zipName = QFileDialog::getSaveFileName(
            this, tr("Select ZIP file"), currentDir(), tr("ZIP files (*.zip)"));
        if (zipName.isEmpty()) return;
        updateCurrentDir(zipName);
        if (!zip.create(zipName)) return;
    } else {
        dirPath = QFileDialog::getExistingDirectory(this, tr("Select export directory"), currentDir());
        if (dirPath.isEmpty()) return;
        updateCurrentDir(dirPath);
    }
    QDir dir(dirPath);
    QString currentPath;
    QString csvName = "Presets-" + QDateTime::currentDateTime().toString("yyMMdd-hhmmss") + ".csv";
    QStringList csvList;
    QSqlQuery query(db);
    query.prepare(
        "SELECT presets.name, presets.instrument, presets.category, presets.character, presets.recorded, models.name, banks.name "
        "FROM presets, models, banks "
        "WHERE presets.id=? AND banks.id=presets.bank AND models.id=banks.model");
    QSqlQuery groupQuery(db);
    groupQuery.prepare("SELECT groups.name FROM presetgroups, groups "
        "WHERE presetgroups.preset=? AND groups.id=presetgroups.groupid "
        "ORDER BY groups.name");
    foreach (const QVariant& id, ids) {
        query.bindValue(0, id);
        if (!execSqlQuery(query)) return;
        if (query.first()) {
            QString preset = query.value(0).toString();
            int instrument = query.value(1).toInt();
            int category = query.value(2).toInt();
            int character = query.value(3).toInt();
            int recorded = query.value(4).toInt();
            QString model = query.value(5).toString();
            QString bank = query.value(6).toString();
            QString path = toValidFileName(model) + "/" + toValidFileName(bank);
            if (path != currentPath) {
                saveCsvFile(csvList, currentPath, csvName, zip, dir);
                currentPath = path;
                csvList.clear();
                csvList.append("sep=,");
                csvList.append(escapeCsvField(tr("MODEL")) + ',' + escapeCsvField(tr("BANK")) + ',' +
                    escapeCsvField(tr("PRESET")) + ',' + escapeCsvField(tr("INSTRUMENT")) + ',' +
                    escapeCsvField(tr("CATEGORY")) + ',' + escapeCsvField(tr("CHARACTER")) + ',' +
                    escapeCsvField(tr("GROUPS")));
            }
            if (recorded != RecNone) {
                QString mp3Path = path + "/" + toValidFileName(preset) + ".mp3";
                if (zip.isOpen()) {
                    zip.addFile(mp3Path, getMP3FilePath(id));
                } else {
                    dir.mkpath(path);
                    QFile file(getMP3FilePath(id));
                    file.copy(dir.absoluteFilePath(mp3Path));
                }
            }
            QString csv = escapeCsvField(model);
            csv += ',' + escapeCsvField(bank);
            csv += ',' + escapeCsvField(preset);
            csv += ',' + escapeCsvField(getListMaskText(ui->selInstrument, instrument));
            csv += ',' + escapeCsvField(getListMaskText(ui->selCategory, category));
            csv += ',' + escapeCsvField(getListMaskText(ui->selCharacter, character));
            groupQuery.bindValue(0, id);
            if (!execSqlQuery(groupQuery)) return;
            QString s;
            while (groupQuery.next()) {
                if (!s.isEmpty()) s += ", ";
                s += groupQuery.value(0).toString();
            }
            csv += ',' + escapeCsvField(s);
            csvList.append(csv);
        }
    }
    saveCsvFile(csvList, currentPath, csvName, zip, dir);
#endif
}

void MainWindow::eximOpenContainer()
{
    QString name = QFileDialog::getOpenFileName(
        this, tr("Select container file"), currentDir(), tr("Patchdirector containers (*.padi)"));
    if (name.isEmpty()) return;
    updateCurrentDir(name);
    MainWindow* wnd = new MainWindow(Q_NULLPTR, true);
    if (!wnd->openContainer(name)) {
        delete wnd;
        QMessageBox::critical(this, QCoreApplication::applicationName(), tr("The container could not be opened."));
        return;
    }
    wnd->setWindowTitle(wnd->windowTitle() + " - " + QFileInfo(name).fileName());
    wnd->ui->eximImportContainer->setEnabled(true);
    wnd->ui->eximCloseContainer->setEnabled(true);
    wnd->ui->recRecord->setEnabled(false);
    wnd->ui->recRecordWM->setEnabled(false);
    wnd->ui->selResDeletePresets->setEnabled(true);
    wnd->ui->selPresetsWithoutAudio->setChecked(true);
    wnd->ui->prefLanguage->setEnabled(false);
    wnd->ui->prefChangeDBDir->setEnabled(false);
    wnd->ui->tabWidget->setCurrentWidget(wnd->ui->tabExImport);
    wnd->tabChanged();
    MainWindow::setMainWnd(wnd);
    QByteArray info;
    if (wnd->container.extractFile("Info.txt", info)) {
        QScopedPointer<EditorDialog> editor(new EditorDialog(wnd));
        editor->setWindowTitle(tr("Container Info"));
        editor->setReadOnly(true);
        editor->setText(QString::fromUtf8(info));
        editor->exec();
    }
    if (wnd->containerDuplicates > 0) {
        wnd->ui->tabWidget->setCurrentWidget(wnd->ui->tabSelect);
        wnd->tabChanged();
        int i = wnd->ui->selGroup->findText(duplicatesGroupName());
        if (i > 0) wnd->ui->selGroup->setCurrentIndex(i);
        QMessageBox::information(this, QCoreApplication::applicationName(),
            tr("Duplicates have been found in the container and are shown here in the group <Duplicates>. "
               "Presets that are not to be imported must be deleted (but still remain in the container). "
               "Then return to the ex-/import-tab and finish the import."));
    }
}

void MainWindow::eximImportContainer()
{
    if (!container.isOpen()) return;
    if (!updateDuplicatesGroup()) return;
    QString text = tr("Do you want to import the container into your database?");
    if (containerDuplicates > 0) {
        text += "\n" + tr("WARNING: %1 duplicate(s) will overwrite existing preset(s)!").arg(containerDuplicates);
    }
    if (QMessageBox::question(this, QCoreApplication::applicationName(), text) != QMessageBox::Yes) return;
    QSqlQuery query(db);
    QString containerGroup = QFileInfo(container.name()).completeBaseName().trimmed();
    if (!containerGroup.isEmpty()) {
        query.prepare("INSERT OR IGNORE INTO groups (name) VALUES (?)");
        query.addBindValue(containerGroup);
        if (!execSqlQuery(query)) return;
        query.prepare(
            "INSERT OR IGNORE INTO presetgroups (preset, groupid) "
            "SELECT presets.id, groups.id FROM presets, groups WHERE groups.name=?");
        query.addBindValue(containerGroup);
        if (!execSqlQuery(query)) return;
    }
    if (!db.transaction()) {
        showSqlError(db.lastError());
        return;
    }
    QSqlQuery queryPreset(db);
    queryPreset.prepare("SELECT id FROM db.presets WHERE bank=:bank AND name=:name");
    QSqlQuery insertPreset(db);
    insertPreset.prepare("INSERT INTO db.presets (bank, name, position) VALUES (:bank, :name, :position)");
    QSqlQuery updatePreset(db);
    updatePreset.prepare(
        "UPDATE db.presets SET instrument=:instrument, category=:category, character=:character, recorded=:recorded "
        "WHERE id=:id");
    QSqlQuery addGroups(db);
    addGroups.prepare(
        "INSERT OR IGNORE INTO db.groups (name) "
        "SELECT groups.name FROM presetgroups, groups "
        "WHERE presetgroups.preset=:id1 AND groups.id=presetgroups.groupid AND groups.name<>:duplicates");
    addGroups.bindValue(":duplicates", duplicatesGroupName());
    QSqlQuery setGroups(db);
    setGroups.prepare(
        "INSERT OR IGNORE INTO db.presetgroups (preset, groupid) "
        "SELECT :id2, g2.id FROM presetgroups, groups AS g1, db.groups AS g2 "
        "WHERE presetgroups.preset=:id1 AND g1.id=presetgroups.groupid AND g1.name<>:duplicates AND g2.name=g1.name");
    setGroups.bindValue(":duplicates", duplicatesGroupName());
    query.prepare("SELECT id, bank, name, instrument, category, character, recorded FROM presets ORDER BY bank, position");
    bool ok = execSqlQuery(query);
    QVariant bank, dbBank;
    int position = 0;
    QStringList audioFiles;
    while (ok && query.next()) {
        QVariant id1 = query.value(0);
        if (!bank.isValid() || bank != query.value(1)) {
            bank = query.value(1);
            QSqlQuery query2(db);
            query2.prepare("SELECT models.name, banks.name FROM models, banks WHERE banks.id=? AND models.id=banks.model");
            query2.addBindValue(bank);
            if (!(ok = execSqlQuery(query2))) break;
            if (query2.first()) {
                QString modelName = query2.value(0).toString();
                QString bankName = query2.value(1).toString();
                query2.prepare("INSERT OR IGNORE INTO db.models (name) VALUES (?)");
                query2.addBindValue(modelName);
                if (!(ok = execSqlQuery(query2))) break;
                query2.prepare("INSERT OR IGNORE INTO db.banks (model, name) VALUES ((SELECT id FROM db.models WHERE name=?), ?)");
                query2.addBindValue(modelName);
                query2.addBindValue(bankName);
                if (!(ok = execSqlQuery(query2))) break;
                query2.prepare("SELECT banks.id FROM db.models, db.banks WHERE models.name=? AND banks.model=models.id AND banks.name=?");
                query2.addBindValue(modelName);
                query2.addBindValue(bankName);
                if (!(ok = execSqlQuery(query2))) break;
                dbBank.clear();
                if (query2.first()) dbBank = query2.value(0);
                queryPreset.bindValue(":bank", dbBank);
                insertPreset.bindValue(":bank", dbBank);
                updatePreset.bindValue(":bank", dbBank);
            }
            query2.prepare("SELECT MAX(position) FROM db.presets WHERE bank=?");
            query2.addBindValue(dbBank);
            if (!(ok = execSqlQuery(query2))) break;
            position = 0;
            if (query2.first()) position = query2.value(0).toInt();
        }
        QString name = query.value(2).toString();
        int recorded = query.value(6).toInt();
        queryPreset.bindValue(":name", name);
        if (!(ok = execSqlQuery(queryPreset))) break;
        QVariant id2;
        if (queryPreset.first()) {
            if (recorded == RecNone) continue;
            id2 = queryPreset.value(0);
        } else {
            insertPreset.bindValue(":name", name);
            insertPreset.bindValue(":position", ++position);
            if (!(ok = execSqlQuery(insertPreset))) break;
            id2 = insertPreset.lastInsertId();
        }
        updatePreset.bindValue(":id", id2);
        updatePreset.bindValue(":instrument", query.value(3));
        updatePreset.bindValue(":category", query.value(4));
        updatePreset.bindValue(":character", query.value(5));
        updatePreset.bindValue(":recorded", recorded);
        if (!(ok = execSqlQuery(updatePreset))) break;
        addGroups.bindValue(":id1", id1);
        if (!(ok = execSqlQuery(addGroups))) break;
        setGroups.bindValue(":id1", id1);
        setGroups.bindValue(":id2", id2);
        if (!(ok = execSqlQuery(setGroups))) break;
        if (recorded != RecNone) {
            QString src = getMP3FilePath(id1);
            QString dest = getPersistentMP3FilePath(id2);
            if (!QFile::exists(src)) {
                ok = false;
                break;
            }
            QFile::remove(dest);
            if (!QFile::copy(src, dest)) {
                ok = false;
                QMessageBox::critical(this, QCoreApplication::applicationName(),
                    tr("The audio file could not be copied."));
            }
            audioFiles.append(dest);
        }
    }
    if (ok) {
        ok = db.commit();
        if (!ok) showSqlError(db.lastError());
    }
    if (!ok) {
        db.rollback();
        foreach (QString file, audioFiles) QFile::remove(file);
    } else {
        QMessageBox::information(this, QCoreApplication::applicationName(),
            tr("Import finished."));
        eximCloseContainer();
    }
}

void MainWindow::eximCloseContainer()
{
    if (!container.isOpen()) return;
    MainWindow* wnd = new MainWindow();
    wnd->ui->tabWidget->setCurrentWidget(wnd->ui->tabExImport);
    MainWindow::setMainWnd(wnd);
}

void MainWindow::prefLanguageChanged()
{
    QSettings settings;
    settings.setValue("Language", ui->prefLanguage->currentData().toString());
    loadTranslation();
    MainWindow* wnd = new MainWindow(Q_NULLPTR);
    wnd->ui->tabWidget->setCurrentWidget(wnd->ui->tabPreferences);
    MainWindow::setMainWnd(wnd);
}

void MainWindow::prefChangeDatabaseDir()
{
    if (container.isOpen()) return;
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select database directory"), currentDir());
    if (dir.isEmpty()) return;
    updateCurrentDir(dir);
    QSettings settings;
    settings.setValue("DatabaseDirectory", dir);
    MainWindow* wnd = new MainWindow(Q_NULLPTR);
    wnd->ui->tabWidget->setCurrentWidget(wnd->ui->tabPreferences);
    MainWindow::setMainWnd(wnd);
}

void MainWindow::prefAudioInputChanged()
{
    QString name;
    if (ui->prefAudioInput->currentIndex() >= 1) name = ui->prefAudioInput->currentText();
    AudioRecorder::setInputDeviceName(name);
}

void MainWindow::prefRecordAudioOutputChanged()
{
#ifdef Q_OS_OSX
    QString name;
    if (ui->prefRecordAudioOutput->currentIndex() >= 1) name = ui->prefRecordAudioOutput->currentText();
    AudioRecorder::setDefaultOutputDeviceName(name);
#endif
}

void MainWindow::prefMP3BitrateChanged()
{
    if (ui->prefMP3Bitrate128->isChecked()) AudioRecorder::setMP3Bitrate(128);
    if (ui->prefMP3Bitrate160->isChecked()) AudioRecorder::setMP3Bitrate(160);
}

void MainWindow::prefUpdateModels()
{
    if (!prefModelsModel) {
        prefModelsModel = new AutoFetchAll<QSqlTableModel>(this, db);
        prefModelsModel->setTable("models");
        prefModelsModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
        connect(prefModelsModel, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)), SLOT(prefModelsDataChanged(QModelIndex,QModelIndex)));
        ui->prefModels->setModel(createSortFilter(prefModelsModel, 1));
        ui->prefModels->setModelColumn(1);
        connect(ui->prefModels->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)), SLOT(prefUpdateBanks()));
    }
    QVariant id = getCurrentItem(ui->prefModels, 0);
    prefModelsModel->select();
    if (id.isValid()) setCurrentItem(ui->prefModels, 0, id);
    prefUpdateBanks();
}

void MainWindow::prefAddModel()
{
    addItem(ui->prefModels, 1, tr("New Model"));
}

void MainWindow::prefRemoveModel()
{
    removeCurrentItem(ui->prefModels, prefModelsModel);
    prefUpdateBanks();
    deleteMP3Files();
}

void MainWindow::prefUpdateBanks()
{
    if (!prefBanksModel) {
        prefBanksModel = new AutoFetchAll<QSqlTableModel>(this, db);
        prefBanksModel->setTable("banks");
        prefBanksModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
        connect(prefBanksModel, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)), SLOT(prefBanksDataChanged(QModelIndex,QModelIndex)));
        ui->prefBanks->setModel(createSortFilter(prefBanksModel, 2));
        ui->prefBanks->setModelColumn(2);
        connect(ui->prefBanks->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)), SLOT(prefUpdatePresets()));
    }
    QVariant id = getCurrentItem(ui->prefBanks, 0);
    QVariant model = getCurrentItem(ui->prefModels, 0);
    if (!model.isValid()) model = -1;
    prefBanksModel->setFilter("model=" + model.toString());
    prefBanksModel->select();
    if (id.isValid()) setCurrentItem(ui->prefBanks, 0, id);
    prefUpdatePresets();
}

void MainWindow::prefAddBank()
{
    QVariant id = getCurrentItem(ui->prefModels, 0);
    if (id.isValid()) addItem(ui->prefBanks, 2, tr("New Bank"), 1, id);
}

void MainWindow::prefRemoveBank()
{
    removeCurrentItem(ui->prefBanks, prefBanksModel);
    deleteMP3Files();
}

void MainWindow::prefEditBanks()
{
    QVariant model = getCurrentItem(ui->prefModels, 0);
    if (!model.isValid()) return;
    QSqlQuery query(db);
    query.prepare("SELECT id, name FROM banks WHERE model=? ORDER BY name");
    query.addBindValue(model);
    if (!execSqlQuery(query)) return;
    QStringList banks;
    QSet<int> bankIDs;
    while (query.next()) {
        banks.append(query.value(1).toString());
        bankIDs.insert(query.value(0).toInt());
    }
    QScopedPointer<EditorDialog> editor(new EditorDialog(this));
    editor->setWindowTitle(tr("Edit Banks"));
    editor->setWordWrap(false);
    editor->setText(banks.join('\n'));
    if (editor->exec() != QDialog::Accepted) return;
    if (!db.transaction()) {
        showSqlError(db.lastError());
        return;
    }
    QSqlQuery insert(db);
    insert.prepare("INSERT OR IGNORE INTO banks (model, name) VALUES (:model, :name)");
    insert.bindValue(":model", model);
    query.prepare("SELECT id FROM banks WHERE model=:model AND name=:name");
    query.bindValue(":model", model);
    banks = editor->text().replace('\t', ' ').replace('\r', '\n').split('\n', QString::SkipEmptyParts);
    bool ok = true;
    foreach (QString bank, banks) {
        insert.bindValue(":name", bank);
        if (!(ok = execSqlQuery(insert))) break;
        query.bindValue(":name", bank);
        if (!(ok = execSqlQuery(query) && query.first())) break;
        bankIDs.remove(query.value(0).toInt());
    }
    if (ok && !bankIDs.isEmpty()) {
        if (QMessageBox::question(this, QCoreApplication::applicationName(),
            tr("Do you really want to delete %1 bank(s)?").arg(bankIDs.size()))
            != QMessageBox::Yes) ok = false;
    }
    if (ok) {
        query.prepare("DELETE FROM banks WHERE id=:id");
        foreach (int id, bankIDs) {
            query.bindValue(":id", id);
            if (!(ok = execSqlQuery(query))) break;
        }
    }
    if (ok) {
        ok = db.commit();
        if (!ok) showSqlError(db.lastError());
    }
    if (!ok) db.rollback();
    prefUpdateBanks();
    deleteMP3Files();
}

void MainWindow::prefUpdatePresets()
{
    if (!prefPresetsModel) {
        prefPresetsModel = new AutoFetchAll<QSqlTableModel>(this, db);
        prefPresetsModel->setTable("presets");
        prefPresetsModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
        connect(prefPresetsModel, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)), SLOT(prefPresetsDataChanged(QModelIndex,QModelIndex)));
        ui->prefPresets->setModel(createSortFilter(prefPresetsModel, 3));
        ui->prefPresets->setModelColumn(2);
    }
    QVariant bank = getCurrentItem(ui->prefBanks, 0);
    if (!bank.isValid()) bank = -1;
    prefPresetsModel->setFilter("bank=" + bank.toString());
    prefPresetsModel->select();
}

void MainWindow::prefAddPreset()
{
    QVariant id = getCurrentItem(ui->prefBanks, 0);
    if (id.isValid()) addItem(ui->prefPresets, 2, tr("New Preset"), 1, id, 3);
}

void MainWindow::prefRemovePreset()
{
    removeCurrentItem(ui->prefPresets, prefPresetsModel);
    deleteMP3Files();
}

void MainWindow::prefEditPresets()
{
    QVariant bank = getCurrentItem(ui->prefBanks, 0);
    if (!bank.isValid()) return;
    QSqlQuery query(db);
    query.prepare("SELECT name FROM presets WHERE bank=? ORDER BY position");
    query.addBindValue(bank);
    if (!execSqlQuery(query)) return;
    QStringList presets;
    while (query.next()) {
        presets.append(query.value(0).toString());
    }
    QScopedPointer<EditorDialog> editor(new EditorDialog(this));
    editor->setWindowTitle(tr("Edit Presets"));
    editor->setWordWrap(false);
    editor->setText(presets.join('\n'));
    if (editor->exec() != QDialog::Accepted) return;
    if (!db.transaction()) {
        showSqlError(db.lastError());
        return;
    }
    query.prepare("UPDATE presets SET position=0 WHERE bank=?");
    query.addBindValue(bank);
    bool ok = execSqlQuery(query);
    if (ok) {
        QSqlQuery insert(db);
        insert.prepare("INSERT OR IGNORE INTO presets (bank, name, position) VALUES (:bank, :name, 0)");
        QSqlQuery update(db);
        update.prepare("UPDATE presets SET position=:position WHERE bank=:bank AND position=0 AND name=:name");
        insert.bindValue(":bank", bank);
        update.bindValue(":bank", bank);
        int position = 0;
        presets = editor->text().replace('\t', ' ').replace('\r', '\n').split('\n', QString::SkipEmptyParts);
        foreach (QString preset, presets) {
            insert.bindValue(":name", preset);
            if (!(ok = execSqlQuery(insert))) break;
            update.bindValue(":name", preset);
            update.bindValue(":position", ++position);
            if (!(ok = execSqlQuery(update))) break;
        }
    }
    if (ok) {
        query.prepare("DELETE FROM presets WHERE bank=? AND position=0");
        query.addBindValue(bank);
        ok = execSqlQuery(query);
    }
    if (ok && query.numRowsAffected() > 0) {
        if (QMessageBox::question(this, QCoreApplication::applicationName(),
            tr("Do you really want to delete %1 preset(s)?").arg(query.numRowsAffected()))
            != QMessageBox::Yes) ok = false;
    }
    if (ok) {
        ok = db.commit();
        if (!ok) showSqlError(db.lastError());
    }
    if (!ok) db.rollback();
    prefPresetsModel->select();
    deleteMP3Files();
}

void MainWindow::prefImportPresets()
{
    QVariant bank = getCurrentItem(ui->prefBanks, 0);
    if (!bank.isValid()) return;
    int position = 0;
    QAbstractItemModel* model = ui->prefPresets->model();
    if (model->rowCount() > 0) {
        position = model->data(model->index(model->rowCount() - 1, 3)).toInt();
    }
    QString filename = QFileDialog::getOpenFileName(
        this, tr("Select preset file"), currentDir(), tr("Text files (*.txt);;All files (*.*)"));
    if (filename.isEmpty()) return;
    updateCurrentDir(filename);
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QTextStream in(&file);
    if (!db.transaction()) {
        showSqlError(db.lastError());
        return;
    }
    QSqlQuery query(db);
    query.prepare("INSERT OR IGNORE INTO presets (bank, name, position) VALUES (:bank, :name, :position)");
    query.bindValue(":bank", bank);
    bool ok = true;
    while (ok && !in.atEnd()) {
        QString name = in.readLine().simplified();
        if (!name.isEmpty()) {
            query.bindValue(":name", name);
            query.bindValue(":position", ++position);
            ok = execSqlQuery(query);
        }
    }
    if (ok) {
        ok = db.commit();
        if (!ok) showSqlError(db.lastError());
    }
    if (!ok) db.rollback();
    prefPresetsModel->select();
}

void MainWindow::prefUpdateGroups()
{
    if (!prefGroupsModel) {
        prefGroupsModel = new AutoFetchAll<QSqlTableModel>(this, db);
        prefGroupsModel->setTable("groups");
        prefGroupsModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
        connect(prefGroupsModel, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)), SLOT(prefGroupsDataChanged(QModelIndex,QModelIndex)));
        ui->prefGroups->setModel(createSortFilter(prefGroupsModel, 1));
        ui->prefGroups->setModelColumn(1);
    }
    prefGroupsModel->select();
}

void MainWindow::prefAddGroup()
{
    addItem(ui->prefGroups, 1, tr("New Group"));
}

void MainWindow::prefRemoveGroup()
{
    removeCurrentItem(ui->prefGroups, prefGroupsModel);
}

void MainWindow::prefRemoveGroupPresets()
{
    QVariant id = getCurrentItem(ui->prefGroups, 0);
    if (!id.isValid()) return;
    if (QMessageBox::question(this, QCoreApplication::applicationName(),
        tr("Do you really want to delete group \"%1\" with presets?\n"
            "If presets still exist in other groups they will be kept. "
            "Otherwise they will now be deleted including audio.")
            .arg(getCurrentItem(ui->prefGroups, 1).toString()))
        != QMessageBox::Yes) return;
    if (!db.transaction()) {
        showSqlError(db.lastError());
        return;
    }
    QSqlQuery query(db);
    query.prepare("DELETE FROM presets WHERE "
        "id IN (SELECT preset FROM presetgroups WHERE groupid=:id) AND "
        "NOT EXISTS (SELECT groupid FROM presetgroups WHERE preset=presets.id AND groupid<>:id)");
    query.bindValue(":id", id);
    bool ok = execSqlQuery(query);
    if (ok) {
        query.prepare("DELETE FROM groups WHERE id=:id");
        query.bindValue(":id", id);
        ok = execSqlQuery(query);
    }
    if (ok) {
        ok = db.commit();
        if (!ok) showSqlError(db.lastError());
    }
    if (!ok) db.rollback();
    prefUpdateGroups();
}

void MainWindow::prefModelsDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    QVariant id = getCurrentItem(ui->prefModels, 0);
    prefModelsModel->submitAll();
    prefModelsModel->select();
    if (id.isValid()) setCurrentItem(ui->prefModels, 0, id);
    ui->prefModels->setFocus();
}

void MainWindow::prefBanksDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    if (topLeft.column() == 1) return;
    QVariant id = getCurrentItem(ui->prefBanks, 0);
    prefBanksModel->submitAll();
    prefBanksModel->select();
    if (id.isValid()) setCurrentItem(ui->prefBanks, 0, id);
    ui->prefBanks->setFocus();
}

void MainWindow::prefPresetsDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    if (topLeft.column() == 1 || topLeft.column() == 3) return;
    QVariant id = getCurrentItem(ui->prefPresets, 0);
    prefPresetsModel->submitAll();
    prefPresetsModel->select();
    if (id.isValid()) setCurrentItem(ui->prefPresets, 0, id);
    ui->prefPresets->setFocus();
}

void MainWindow::prefGroupsDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    QVariant id = getCurrentItem(ui->prefGroups, 0);
    prefGroupsModel->submitAll();
    prefGroupsModel->select();
    if (id.isValid()) setCurrentItem(ui->prefGroups, 0, id);
    ui->prefGroups->setFocus();
}

void MainWindow::aboutUrlHandler(const QUrl &url)
{
    if (url.fileName() == "license") {
        LicenseDialog dlg(this);
        dlg.exec();
    } else if (url.fileName() == "qt") {
        QMessageBox::aboutQt(this);
    }
}
