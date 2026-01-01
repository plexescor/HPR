#include "timeUtils.hpp"

namespace timeUtils
{

    //timeFormat is a struct, see the header file for its definition
    timeFormat formatSeconds(double totalSeconds)
    {
        int total = static_cast<int>(totalSeconds);
        
        timeFormat t;
        t.hours = total / 3600;
        t.minutes = (total % 3600) / 60;
        t.seconds = total % 60;
        
        return t;
    }
}

