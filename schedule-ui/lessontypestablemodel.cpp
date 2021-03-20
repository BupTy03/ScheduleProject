#include "lessontypestablemodel.hpp"
#include "utils.hpp"
#include "scheduleimportexport.hpp"

#include <map>
#include <numeric>


static constexpr auto DEFAULT_COLUMNS_COUNT = 4;

enum class ColumnType
{
    Name,
    Hours,
    Days,
    Classrooms
};


QString WeekDaysString(const WeekDaysType& weekDays)
{
    static const std::array<QString, 6> daysNames = {
        QObject::tr("ПН"),
        QObject::tr("ВТ"),
        QObject::tr("СР"),
        QObject::tr("ЧТ"),
        QObject::tr("ПТ"),
        QObject::tr("СБ")
    };

    QString daysStr;
    for (std::size_t d = 0; d < weekDays.size(); ++d)
    {
        if (weekDays.at(d))
        {
            if (!daysStr.isEmpty())
                daysStr.push_back(", ");

            daysStr.push_back(daysNames.at(d));
        }
    }

    return daysStr;
}

QString ToString(const LessonTypeItem& lesson)
{
    return QString("%1 (%2) %3 [%4]")
            .arg(lesson.Name)
            .arg(lesson.CountHoursPerWeek)
            .arg(WeekDaysString(lesson.WeekDays))
            .arg(Join(lesson.Classrooms, ", "));
}

DisciplineValidationResult Validate(const Discipline& discipline)
{
    if(discipline.Professor.isEmpty())
        return DisciplineValidationResult::NoProfessor;

    if(discipline.Name.isEmpty())
        return DisciplineValidationResult::NoName;

    if(HoursPerWeekSum(discipline.Lessons) <= 0)
        return DisciplineValidationResult::NoLessons;

    if(discipline.Groups.empty())
        return DisciplineValidationResult::NoGroups;

    return DisciplineValidationResult::Ok;
}

QString ToWarningMessage(DisciplineValidationResult validationResult)
{
    static const std::map<DisciplineValidationResult, QString> mapping = {
            {DisciplineValidationResult::Ok, QObject::tr("Ok")},
            {DisciplineValidationResult::NoName, QObject::tr("Необходимо указать название дисциплины")},
            {DisciplineValidationResult::NoProfessor, QObject::tr("Необходимо выбрать преподавателя")},
            {DisciplineValidationResult::NoGroups, QObject::tr("Необходимо выбрать группы")},
            {DisciplineValidationResult::NoLessons, QObject::tr("Необходимо назначить часы для хотя бы одного типа занятий")}
    };

    auto it = mapping.find(validationResult);
    if(it == mapping.end())
    {
        assert(false && "Unknown enum value");
        return QObject::tr("Неизвестная ошибка");
    }

    return it->second;
}

int HoursPerWeekSum(const std::vector<LessonTypeItem>& lessons)
{
    return std::accumulate(lessons.begin(), lessons.end(), 0, [](int lhs, const LessonTypeItem& rhs){
        return lhs + rhs.CountHoursPerWeek;
    });
}


LessonTypesTableModel::LessonTypesTableModel(QObject* parent)
    : QAbstractTableModel(parent)
    , lessons_()
{
    lessons_.emplace_back(tr("Лекция"), 0,
                          WeekDaysType{ true, true, true, true, true, true },
                          ClassroomsSet{});

    lessons_.emplace_back(tr("Практика"), 0,
                          WeekDaysType{ true, true, true, true, true, true },
                          ClassroomsSet{});
    lessons_.emplace_back(tr("Лабораторная"), 0,
                          WeekDaysType{ true, true, true, true, true, true },
                          ClassroomsSet{});
}

const std::vector<LessonTypeItem>& LessonTypesTableModel::lessons() const
{
    return lessons_;
}

int LessonTypesTableModel::rowCount(const QModelIndex&) const
{
    return static_cast<int>(lessons_.size());
}

int LessonTypesTableModel::columnCount(const QModelIndex&) const
{
    return DEFAULT_COLUMNS_COUNT;
}

QVariant LessonTypesTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (Qt::ItemDataRole::DisplayRole != role)
        return {};

    if (Qt::Orientation::Vertical == orientation)
        return {};

    static const std::array<QString, DEFAULT_COLUMNS_COUNT> sections = {
        tr("Тип"), tr("Часов в неделю"), tr("Дни"), tr("Аудитории")
    };
    return sections.at(static_cast<std::size_t>(section));
}

QVariant LessonTypesTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};

    const auto& lesson = lessons_.at(static_cast<std::size_t>(index.row()));
    const auto columnType = static_cast<ColumnType>(index.column());

    switch (role)
    {
    case Qt::ItemDataRole::DisplayRole:
    {
        switch (columnType)
        {
        case ColumnType::Name:
            return lesson.Name;
        case ColumnType::Hours:
            return lesson.CountHoursPerWeek;
        case ColumnType::Days:
            return WeekDaysString(lesson.WeekDays);
        case ColumnType::Classrooms:
            return Join(lesson.Classrooms, ", ");
        }
        break;
    }
    case Qt::ItemDataRole::UserRole:
    {
        if (columnType == ColumnType::Days)
            return QVariant::fromValue(lesson.WeekDays);

        if(columnType == ColumnType::Classrooms)
            return QVariant::fromValue(lesson.Classrooms);

        break;
    }
    case Qt::ItemDataRole::TextAlignmentRole:
    {
        switch (columnType)
        {
        case ColumnType::Name:
        case ColumnType::Hours:
            return Qt::AlignmentFlag::AlignCenter;
        case ColumnType::Days:
        case ColumnType::Classrooms:
            return static_cast<int>(Qt::AlignmentFlag::AlignVCenter | Qt::AlignmentFlag::AlignLeft);
        }
        break;
    }
    }

    return {};
}

bool LessonTypesTableModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid())
        return false;

    if (Qt::ItemDataRole::EditRole != role)
        return false;

    auto& lesson = lessons_.at(static_cast<std::size_t>(index.row()));
    switch (static_cast<ColumnType>(index.column()))
    {
    case ColumnType::Name:
        lesson.Name = value.toString();
        break;
    case ColumnType::Hours:
        lesson.CountHoursPerWeek = value.toInt();
        break;
    case ColumnType::Days:
        lesson.WeekDays = value.value<WeekDaysType>();
        break;
    case ColumnType::Classrooms:
        lesson.Classrooms = value.value<ClassroomsSet>();
        break;
    }

    return true;
}

Qt::ItemFlags LessonTypesTableModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::ItemFlag::NoItemFlags;

    if (index.column() == 0)
        return Qt::ItemFlag::ItemIsEnabled | Qt::ItemFlag::ItemIsSelectable;

    return Qt::ItemFlag::ItemIsEnabled | Qt::ItemFlag::ItemIsSelectable | Qt::ItemFlag::ItemIsEditable;
}
