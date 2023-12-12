/**
* Copyright (c) 2005-2006, Applied Informatics Software Engineering GmbH. and Contributors.
* SPDX-License-Identifier:	BSL-1.0
*
*/
#include <Common/NIO/SocketNotification.h>
#include <Common/NIO/SocketNotifier.h>
#include <Common/NIO/SocketReactor.h>


namespace RK
{

SocketNotifier::SocketNotifier(const Socket & socket_) : socket(socket_)
{
}

bool SocketNotifier::addObserverIfNotExist(const AbstractObserver & observer)
{
    return nc.addObserverIfNotExist(observer);
}


bool SocketNotifier::removeObserverIfExist(const AbstractObserver & observer)
{
    return nc.removeObserverIfExist(observer);
}


void SocketNotifier::dispatch(const Notification & notification)
{
    try
    {
        nc.postNotification(notification);
    }
    catch (...)
    {
        throw;
    }
}

}
