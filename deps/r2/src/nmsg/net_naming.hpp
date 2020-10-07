#pragma once

#include "../common.hpp"

namespace r2
{

/*!
    AddrID encodes the abstract ID of each machine
    It includes 2 parts:
    -  the machine ID
    -  the thread ID

    usage:
    AddrID id(mac_id,thread_id);
    LOG(2) << id;

    It is also convenient to us this id as vanilla integer,
    such that:
    u32 res = id.to_u32();
    .....
    
    And this ID can be converted back:
    AddrID id(res);
 */
struct AddrID
{
    u32 mac_id : 16;
    u32 thread_id : 16;

    AddrID() = default;
    inline AddrID(const u32 &m, const u32 &t)
        : mac_id(m), thread_id(t)
    {
    }

    inline AddrID(const u32 &res)
    {
        from_u32(res);
    }

    inline u64 to_u32() const
    {
        return *(reinterpret_cast<const u32 *>(this));
    }

    inline void from_u32(const u32 &res)
    {
        *(reinterpret_cast<u32 *>(this)) = res;
    }

    std::string to_str() const
    {
        std::ostringstream oss;
        oss << "address's mac_id: " << mac_id
            << "; address's thread: " << thread_id;
        return oss.str();
    }

    static u32 max_mac_id_supported()
    {
        return std::numeric_limits<u16>::max();
    }

    static u32 max_thread_id_supported()
    {
        return max_mac_id_supported();
    }
};

/*!
    Test codes
 */
struct AddrIDTester
{
    static void test_encoding()
    {
        for (uint i = 0; i < AddrID::max_mac_id_supported(); ++i)
        {
            for (uint j = 0; j < AddrID::max_thread_id_supported(); ++j)
            {
                AddrID id(i, j);
                auto res = id.to_u32();
                AddrID id1(res);
                ASSERT(res == id1.to_u32());
            }
        }
    }
};

} // namespace r2
