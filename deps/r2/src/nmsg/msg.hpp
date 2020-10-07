#pragma once

#include "../common.hpp"

namespace r2
{

/*!
    Msg is a safe wrapper over a raw pointer.
    To safely create/destory a message,
    please refer to constructors at msg_factory.hpp
 */
class CopyMsg;
class Msg
{
public:
    Msg(char *data, u32 sz) : data(data), size(sz) {}

    /*！
        Provide a method for parsing msg
     */
    template <typename T>
    inline T *interpret_as(const u32 &offset = 0)
    {
        if (unlikely(size - offset < sizeof(T)))
            return nullptr;
        return reinterpret_cast<T *>(data + offset);
    }

    template <typename T>
    inline T *interpret_as_panic(const u32 &offset = 0)
    {
        ASSERT(size >= sizeof(T) + offset);
        return interpret_as<T>(offset);
    }

protected:
    char *data = nullptr;
    const u32 size = 0;

private:
    friend class CopyMsg;
};
} // namespace r2