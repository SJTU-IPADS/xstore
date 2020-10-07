#pragma one

#include "net_naming.hpp"
#include "msg.hpp"

namespace r2
{

class Messanger
{
public:
    // TODO, not think about it carefully
    virtual void in_coming() = 0;
};

} // namespace r2