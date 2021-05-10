#include "ScheduleCommon.hpp"


const std::size_t SCHEDULE_DAYS_COUNT = 12;


WeekDays::WeekDays(std::initializer_list<WeekDay> lst)
{
    if(std::empty(lst))
        return;

    days_ = 0;
    for(auto wd : lst)
        Add(wd);
}

WeekDay ScheduleDayNumberToWeekDay(std::size_t dayNum)
{
    return static_cast<WeekDay>(dayNum % 6);
}

WeekDays::iterator WeekDays::begin() const
{
    if(Empty())
        days_ = FULL_WEEK;

    return WeekDaysIterator(days_, WeekDaysIterator::BEGIN_MASK);
}

WeekDays::iterator WeekDays::end() const { return WeekDaysIterator(days_, WeekDaysIterator::END_MASK); }

std::size_t WeekDays::size() const { return 6; }

void WeekDays::Add(WeekDay d) { days_ |= (1 << static_cast<std::uint8_t>(d)); }

void WeekDays::Remove(WeekDay d) { days_ &= ~(1 << static_cast<std::uint8_t>(d)); }

bool WeekDays::Contains(WeekDay d) const { return Empty() || (days_ & (1 << static_cast<std::uint8_t>(d))); }

bool WeekDays::Empty() const { return days_ == 0; }


std::size_t CalculatePadding(std::size_t baseAddress, std::size_t alignment)
{
    const std::size_t multiplier = (baseAddress / alignment) + 1;
    const std::size_t alignedAddress = multiplier * alignment;
    const std::size_t padding = alignedAddress - baseAddress;
    return padding;
}
