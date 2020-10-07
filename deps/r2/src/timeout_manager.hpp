#pragma once

#include "common.hpp"

#include <queue>

namespace r2
{

/*!
 We use rdtsc(), namely cycles for timeout management;
 All pending routines are add towards a heap, which records its start time 
*/

struct TMElement
{
    u8 cor_id;
    u64 end_time;
    u32 seq; // seq is used to filter out duplicate

    TMElement(const u8 &id, const u64 &end_time, const u32 &s)
        : cor_id(id), end_time(end_time),seq(s) {}
};

struct TECmp
{
    bool operator()(TMElement a, TMElement b)
    {
        return a.end_time > b.end_time;
    }
};

class TMIter;
class TM
{

    friend class TMIter;
    std::priority_queue<TMElement, std::vector<TMElement>, TECmp> q;

public:
    void enqueue(const u8 &id, const u64 &end_time,const u32 &seq)
    {
        q.emplace(id, end_time,seq);
    }
};

class TMIter
{
    TM *tm;
    u64 time;

public:
    TMIter(TM &t, u64 time) : tm(&t), time(time)
    {
    }

    bool valid()
    {
        return (!tm->q.empty()) && tm->q.top().end_time < time;
    }

    std::pair<u8,u32> next()
    {
        auto id = tm->q.top().cor_id;
        auto seq = tm->q.top().seq;
        tm->q.pop();
        return std::make_pair(id,seq);
    }
};

} // namespace r2