#ifndef ROOTCAUSEMODEL_H
#define ROOTCAUSEMODEL_H

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class RootCauseModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap rootCauseSummary READ rootCauseSummary WRITE setRootCauseSummary NOTIFY rootCauseSummaryChanged)
    Q_PROPERTY(QVariantList causalFlowList READ causalFlowList WRITE setCausalFlowList NOTIFY causalFlowListChanged)
    Q_PROPERTY(QVariantList classificationList READ classificationList WRITE setClassificationList NOTIFY classificationListChanged)

public:
    explicit RootCauseModel(QObject *parent = nullptr);

    QVariantMap rootCauseSummary() const { return m_rootCauseSummary; }
    void setRootCauseSummary(const QVariantMap &map);

    QVariantList causalFlowList() const { return m_causalFlowList; }
    void setCausalFlowList(const QVariantList &list);

    QVariantList classificationList() const { return m_classificationList; }
    void setClassificationList(const QVariantList &list);

signals:
    void rootCauseSummaryChanged();
    void causalFlowListChanged();
    void classificationListChanged();

private:
    QVariantMap m_rootCauseSummary;
    QVariantList m_causalFlowList;
    QVariantList m_classificationList;
};

#endif // ROOTCAUSEMODEL_H
