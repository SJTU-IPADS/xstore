#pragma once

#include "msg.hpp"

#include "../allocator_master.hpp"

namespace r2
{

/*!
    AllocMsg uses an allocator (i.e. malloc) to alloc message.
 */
class AllocMsg : public Msg
{
public:
    AllocMsg(Allocator *allocator, u32 size)
        : Msg(allocator ? reinterpret_cast<char *>(allocator->alloc(size)) : nullptr, size),
          alloc(allocator)
    {
        ASSERT(alloc != nullptr);
    }

    explicit AllocMsg(u32 size)
        : AllocMsg(AllocatorMaster<0>::get_thread_allocator(), size) {}

    ~AllocMsg()
    {
        //        LOG(4) << "alloc msg dealloc";
        alloc->dealloc(data);
    }

private:
    Allocator *alloc = nullptr;
};

/*!
    Copy a message to an alloc msg.
 */
class CopyMsg : public AllocMsg
{
public:
    CopyMsg(Allocator *allocator, Msg *msg) : AllocMsg(allocator, msg->size)
    {
        memcpy(data, msg->data, msg->size);
    }

    CopyMsg(Msg *msg) : CopyMsg(AllocatorMaster<0>::get_thread_allocator(), msg)
    {
    }
};

} // namespace r2