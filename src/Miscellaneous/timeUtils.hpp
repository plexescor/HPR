#pragma once

namespace timeUtils
{
    struct timeFormat {
        int hours = 0;
        int minutes = 0;
        int seconds = 0;
    };
    timeFormat formatSeconds(double totalSeconds);
}
