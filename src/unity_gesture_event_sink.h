/*
 * Copyright © 2019 UBports
 *
 */

#ifndef USC_UNITY_GESTURE_EVENT_SINK_H_
#define USC_UNITY_GESTURE_EVENT_SINK_H_

#include "gesture_event_sink.h"
#include "dbus_connection_handle.h"

namespace usc
{

class UnityGestureEventSink : public GestureEventSink
{
public:
   UnityGestureEventSink(std::string const& dbus_address);

   void notify_gesture(std::string const& name) override;

private:
    DBusConnectionHandle dbus_connection;
};

}

#endif
