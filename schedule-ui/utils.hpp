#pragma once
#include "ScheduleCommon.hpp"
#include "ScheduleData.hpp"
#include "scheduleimportexport.hpp"

#include <QPainter>
#include <set>


class QModelIndex;
class QAbstractItemModel;


struct QPainterBrushOriginSaver
{
    explicit QPainterBrushOriginSaver(QPainter& painter);
    ~QPainterBrushOriginSaver();

    QPainterBrushOriginSaver(const QPainterBrushOriginSaver&) = delete;
    QPainterBrushOriginSaver& operator=(const QPainterBrushOriginSaver&) = delete;

private:
    QPainter* painter_;
    const QPointF origin_;
};


struct QPainterStateSaver
{
    explicit QPainterStateSaver(QPainter& painter);
    ~QPainterStateSaver();

    QPainterStateSaver(const QPainterStateSaver&) = delete;
    QPainterStateSaver& operator=(const QPainterStateSaver&) = delete;

private:
    QPainter* const painter_;
};


struct QModelIndexGreaterByRow
{
    bool operator()(const QModelIndex& lhs, const QModelIndex& rhs) const;
};


void CopyRows(const QAbstractItemModel& from, QAbstractItemModel& to);
void MoveRows(QAbstractItemModel& from, QAbstractItemModel& to, QList<QModelIndex> rows);

QString Join(const SortedSet<QString>& strSet, const QString& glue);

std::vector<std::size_t> ToGroupsSet(const QStringList& allGroups, const StringsSet& currentGroups);
std::vector<ClassroomAddress> ToClassroomsSet(const QStringList& allClassrooms, const StringsSet& currentClassrooms);
QString ToWarningMessage(ScheduleDataValidationResult validationResult);
