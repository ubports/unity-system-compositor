/*
 * Copyright © 2019 UBports
 *
 */

#ifndef USC_GESTURE_EVENT_SINK_H_
#define USC_GESTURE_EVENT_SINK_H_

#include <string>

namespace usc
{

class GestureEventSink
{
public:
    virtual ~GestureEventSink() = default;

    virtual void notify_gesture(std::string const& name) = 0;

protected:
    GestureEventSink() = default;
    GestureEventSink(GestureEventSink const&) = delete;
    GestureEventSink& operator=(GestureEventSink const&) = delete;
};

}

#endif
