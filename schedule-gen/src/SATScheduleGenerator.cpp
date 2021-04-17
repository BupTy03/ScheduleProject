#include "SATScheduleGenerator.hpp"
#include "ScheduleCommon.hpp"

#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/filelineiter.h"
#include "ortools/base/logging.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/model.h"

#include <map>
#include <vector>
#include <utility>


using operations_research::sat::BoolVar;
using operations_research::sat::CpModelBuilder;
using operations_research::sat::LinearExpr;
using operations_research::sat::Model;
using operations_research::sat::CpSolverResponse;

using operations_research::sat::SolutionBooleanValue;
using operations_research::sat::NewSatParameters;
using operations_research::sat::NewFeasibleSolutionObserver;
using operations_research::sat::Solve;


// [day, group, professor, lesson, classrooms, subject]
using mtx_index = std::tuple<std::size_t, std::size_t, std::size_t, std::size_t, std::size_t, std::size_t>;


using LessonsMtxItem = std::pair<mtx_index, BoolVar>;

struct LessonsMtxItemComp
{
    bool operator()(const LessonsMtxItem& lhs, const LessonsMtxItem& rhs) const
    {
        return lhs.first < rhs.first;
    }

    bool operator()(const LessonsMtxItem& lhs, const mtx_index& rhs) const
    {
        return lhs.first < rhs;
    }

    bool operator()(const mtx_index& lhs, const LessonsMtxItem& rhs) const
    {
        return lhs < rhs.first;
    }
};


static std::size_t CalculateBooleansCount(const ScheduleData& data)
{
    std::size_t count = 0;
    for (std::size_t d = 0; d < SCHEDULE_DAYS_COUNT; ++d) {
        for (std::size_t g = 0; g < data.CountGroups(); ++g) {
            for (std::size_t p = 0; p < data.CountProfessors(); ++p) {
                for (std::size_t l = 0; l < data.MaxCountLessonsPerDay(); ++l) {
                    for (std::size_t c = 0; c < data.CountClassrooms(); ++c) {
                        for (std::size_t s = 0; s < data.CountSubjects(); ++s) {
                            count += (WeekDayRequestedForSubject(data, s, d) && ClassroomRequestedForSubject(data, s, c));
                        }
                    }
                }
            }
        }
    }
    return count;
}

static void FillLessonsMatrix(CpModelBuilder& cp_model, std::vector<LessonsMtxItem>& mtx, const ScheduleData& data)
{
    for (std::size_t d = 0; d < SCHEDULE_DAYS_COUNT; ++d)
    {
        for (std::size_t g = 0; g < data.CountGroups(); ++g)
        {
            for (std::size_t p = 0; p < data.CountProfessors(); ++p)
            {
                for (std::size_t l = 0; l < data.MaxCountLessonsPerDay(); ++l)
                {
                    for (std::size_t c = 0; c < data.CountClassrooms(); ++c)
                    {
                        for (std::size_t s = 0; s < data.CountSubjects(); ++s)
                        {
                            if (WeekDayRequestedForSubject(data, s, d) &&
                                ClassroomRequestedForSubject(data, s, c) &&
                                !data.LessonIsOccupied(LessonAddress(g, d, l)))
                            {
                                mtx.emplace_back(mtx_index{d, g, p, l, c, s}, cp_model.NewBoolVar());
                            }
                        }
                    }
                }
            }
        }
    }
}

static void AddOneSubjectPerTimeCondition(CpModelBuilder& cp_model,
                                          const std::vector<LessonsMtxItem>& lessons,
                                          const ScheduleData& data,
                                          std::vector<BoolVar>& buffer)
{
    for (std::size_t g = 0; g < data.CountGroups(); ++g) {
        for (std::size_t p = 0; p < data.CountProfessors(); ++p) {
            for (std::size_t d = 0; d < SCHEDULE_DAYS_COUNT; ++d) {
                for (std::size_t l = 0; l < data.MaxCountLessonsPerDay(); ++l) {
                    buffer.clear();
                    for (std::size_t s = 0; s < data.CountSubjects(); ++s) {
                        for (std::size_t c = 0; c < data.CountClassrooms(); ++c) {
                            const mtx_index idx{d, g, p, l, c, s};
                            const auto it = std::lower_bound(lessons.begin(), lessons.end(), idx, LessonsMtxItemComp());
                            if (it != lessons.end() && it->first == idx)
                                buffer.emplace_back(it->second);
                        }
                    }

                    cp_model.AddLessOrEqual(LinearExpr::BooleanSum(buffer), 1);
                }
            }
        }
    }
}

static void AddSubjectsHoursCondition(CpModelBuilder& cp_model,
                                      const std::vector<LessonsMtxItem>& lessons,
                                      const ScheduleData& data,
                                      std::vector<BoolVar>& buffer)
{
    // в сумме для одной группы за весь период должно быть ровно стролько пар, сколько выделено на каждый предмет
    for (std::size_t s = 0; s < data.CountSubjects(); ++s)
    {
        for (std::size_t g = 0; g < data.CountGroups(); ++g)
        {
            for (std::size_t p = 0; p < data.CountProfessors(); ++p)
            {
                buffer.clear();
                for (std::size_t d = 0; d < SCHEDULE_DAYS_COUNT; ++d)
                {
                    for (std::size_t l = 0; l < data.MaxCountLessonsPerDay(); ++l)
                    {
                        for (std::size_t c = 0; c < data.CountClassrooms(); ++c)
                        {
                            const mtx_index idx{d, g, p, l, c, s};
                            const auto it = std::lower_bound(lessons.begin(), lessons.end(), idx, LessonsMtxItemComp());
                            if (it != lessons.end() && it->first == idx)
                                buffer.emplace_back(it->second);
                        }
                    }
                }

                cp_model.AddEquality(LinearExpr::BooleanSum(buffer), CalculateHours(data, p, g, s));
            }
        }
    }
}

static void AddMinimizeLatePairsCondition(CpModelBuilder& cp_model,
                                          const std::vector<LessonsMtxItem>& lessons,
                                          const ScheduleData& data,
                                          std::vector<BoolVar>& buffer)
{
    // располагаем пары в начале дня, стараясь не превышать data.RequestedCountLessonsPerDay()
    buffer.clear();
    std::vector<std::int64_t> pairsCoefficients;
    for(auto&& item : lessons)
    {
        // [day, group, professor, lesson, classrooms, subject]
        const auto[d, g, p, l, c, s] = item.first;

        // чем позднее пара - тем выше коэффициент
        std::int64_t coeff = l;

        // +1 если пара превышает желаемое количество пар в день
        coeff += (l >= data.RequestedCountLessonsPerDay());

        // +1 если пара в субботу
        coeff += (ScheduleDayNumberToWeekDay(d) == WeekDay::Saturday);

        buffer.emplace_back(item.second);
        pairsCoefficients.emplace_back(coeff);
    }

    cp_model.Minimize(LinearExpr::BooleanScalProd(buffer, pairsCoefficients));
}

static void AddMinimizeComplexity(CpModelBuilder& cp_model,
                                  const std::vector<LessonsMtxItem>& lessons,
                                  const ScheduleData& data,
                                  std::vector<BoolVar>& buffer)
{
    for (std::size_t g = 0; g < data.CountGroups(); ++g)
    {
        for (std::size_t d = 0; d < SCHEDULE_DAYS_COUNT; ++d)
        {
            buffer.clear();
            std::vector<std::int64_t> sumComplexity;
            for (std::size_t s = 0; s < data.CountSubjects(); ++s)
            {
                const auto complexity = data.SubjectRequests().at(s).Complexity();
                for (std::size_t l = 0; l < data.MaxCountLessonsPerDay(); ++l)
                {
                    for (std::size_t c = 0; c < data.CountClassrooms(); ++c)
                    {
                        for (std::size_t p = 0; p < data.CountProfessors(); ++p)
                        {
                            const mtx_index idx{d, g, p, l, c, s};
                            const auto it = std::lower_bound(lessons.begin(), lessons.end(), idx, LessonsMtxItemComp());
                            if(it == lessons.end() || it->first != idx)
                                continue;

                            buffer.emplace_back(it->second);
                            sumComplexity.emplace_back(complexity);
                        }
                    }
                }
            }

            if(!buffer.empty())
                cp_model.Minimize(LinearExpr::BooleanScalProd(buffer, sumComplexity));
        }
    }
}

static void AddConditions(CpModelBuilder& cp_model,
                          const std::vector<LessonsMtxItem>& lessons,
                          const ScheduleData& data)
{
    std::vector<BoolVar> buffer;
    AddOneSubjectPerTimeCondition(cp_model, lessons, data, buffer);
    AddSubjectsHoursCondition(cp_model, lessons, data, buffer);
    AddMinimizeComplexity(cp_model, lessons, data, buffer);
    AddMinimizeLatePairsCondition(cp_model, lessons, data, buffer);
}

static ScheduleResult MakeScheduleFromSolverResponse(const CpSolverResponse& response,
                                                       const std::vector<LessonsMtxItem>& lessons,
                                                       const ScheduleData& data)
{
    std::vector<ScheduleResult::Group> resultScheduleGroups;
    for (std::size_t g = 0; g < data.CountGroups(); ++g)
    {
        ScheduleResult::Group resultScheduleGroup;
        for (std::size_t d = 0; d < SCHEDULE_DAYS_COUNT; ++d)
        {
            ScheduleResult::Day resultScheduleDay;
            for (std::size_t l = 0; l < data.MaxCountLessonsPerDay(); ++l)
            {
                ScheduleResult::Lesson resultScheduleLesson = std::nullopt;
                for(std::size_t p = 0; p < data.CountProfessors(); ++p)
                {
                    for (std::size_t c = 0; c < data.CountClassrooms(); ++c)
                    {
                        for (std::size_t s = 0; s < data.CountSubjects(); ++s)
                        {
                            const mtx_index idx{d, g, p, l, c, s};
                            const auto it = std::lower_bound(lessons.begin(), lessons.end(), idx, LessonsMtxItemComp());
                            if (it != lessons.end() && it->first == idx && SolutionBooleanValue(response, it->second))
                            {
                                resultScheduleLesson.emplace(s, p, c);
                            }
                        }
                    }
                }
                resultScheduleDay.emplace_back(std::move(resultScheduleLesson));
            }
            resultScheduleGroup.emplace_back(std::move(resultScheduleDay));
        }
        resultScheduleGroups.emplace_back(std::move(resultScheduleGroup));
    }

    return ScheduleResult(std::move(resultScheduleGroups));
}


ScheduleResult SATScheduleGenerator::Generate(const ScheduleData& data)
{
    CpModelBuilder cp_model;
    std::vector<LessonsMtxItem> lessons;
    lessons.reserve(CalculateBooleansCount(data));

    FillLessonsMatrix(cp_model, lessons, data);
    AddConditions(cp_model, lessons, data);

    const CpSolverResponse response = Solve(cp_model.Build());
    LOG(INFO) << CpSolverResponseStats(response);
    if(!response.IsInitialized())
        return ScheduleResult({});

    return MakeScheduleFromSolverResponse(response, lessons, data);
}
