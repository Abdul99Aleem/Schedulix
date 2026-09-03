#include "RootCauseModel.h"

RootCauseModel::RootCauseModel(QObject *parent)
    : QObject(parent)
{
}

void RootCauseModel::setRootCauseSummary(const QVariantMap &map)
{
    if (m_rootCauseSummary != map) {
        m_rootCauseSummary = map;
        emit rootCauseSummaryChanged();
    }
}

void RootCauseModel::setCausalFlowList(const QVariantList &list)
{
    if (m_causalFlowList != list) {
        m_causalFlowList = list;
        emit causalFlowListChanged();
    }
}

void RootCauseModel::setClassificationList(const QVariantList &list)
{
    if (m_classificationList != list) {
        m_classificationList = list;
        emit classificationListChanged();
    }
}
