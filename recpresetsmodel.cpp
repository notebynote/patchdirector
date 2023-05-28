#include "recpresetsmodel.h"

#include "mainwindow.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

RecPresetsModel::RecPresetsModel(QObject *parent, QSqlDatabase db) :
    QSqlTableModel(parent, db),
    blankIcon("://res/blank-32.png"),
    recIcon("://res/media-record-32.png"),
    recWmIcon("://res/media-record-wm-32.png")
{}

QVariant RecPresetsModel::data(const QModelIndex &index, int role) const
{
    if (index.column() == 2 && role == Qt::DecorationRole) {
        switch (QSqlTableModel::data(this->index(index.row(), 7)).toInt()) {
            case MainWindow::RecNone:      return blankIcon;
            case MainWindow::RecWatermark: return recWmIcon;
            default:                       return recIcon;
        }
    }
    if (role == Qt::ToolTipRole) {
        QSqlQuery query(database());
        query.prepare("SELECT models.name, banks.name FROM models, banks WHERE banks.id=? AND models.id=banks.model");
        query.addBindValue(QSqlTableModel::data(this->index(index.row(), 1)).toInt());
        if (!query.exec() || !query.first()) {
            qDebug() << "RecPresetsModel::data: tooltip query failed. " << query.lastError();
            return "";
        }
        return query.value(0).toString() + "\n" + query.value(1).toString();
    }
    return QSqlTableModel::data(index, role);
}

QString RecPresetsModel::orderByClause() const
{
    return "ORDER BY presets.bank, presets.position";
}
