#ifndef RECPRESETSMODEL_H
#define RECPRESETSMODEL_H

#include <QSqlTableModel>
#include <QIcon>

class RecPresetsModel : public QSqlTableModel
{
    Q_OBJECT
public:
    explicit RecPresetsModel(QObject *parent = 0, QSqlDatabase db = QSqlDatabase());

    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual QString orderByClause() const override;

private:
    QIcon blankIcon;
    QIcon recIcon;
    QIcon recWmIcon;
};

#endif // RECPRESETSMODEL_H
