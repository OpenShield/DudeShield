/****************************************************************************
**
** Copyright (C) 2019 Gianni Peschiutta
** Contact: https://bitbucket.org/Artemia/easymorse/src/master/
**
** FFS2Play is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 3 of the License, or
** (at your option) any later version.
**
** FFS2Play is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** The license is as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this software. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
****************************************************************************/

/****************************************************************************
** singleton.h is part of EasyMorse project
**
** This provide template for singleton instanciation
****************************************************************************/

#ifndef SINGLETON_H
#define SINGLETON_H
#include <QtGlobal>
#include <QScopedPointer>

#include "tools/call_once.h"

template <class T>
class Singleton
{
private:
    typedef T* (*CreateInstanceFunction)();
public:
    static T* instance(CreateInstanceFunction create);
private:
    static void init();

    Singleton();
    ~Singleton();
    Q_DISABLE_COPY(Singleton)
    static QBasicAtomicPointer<void> create;
    static QBasicAtomicInt flag;
    static QBasicAtomicPointer<void> tptr;
    bool inited;
};

template <class T>
T* Singleton<T>::instance(CreateInstanceFunction create)
{
#if QT_VERSION < 0x051400
    Singleton::create.store((T*)create);
#else
    Singleton::create.storeRelaxed((T*)create);
#endif
    qCallOnce(init, flag);
#if QT_VERSION < 0x051400
    return (T*)tptr.load();
#else
    return (T*)tptr.loadRelaxed();
#endif
}

template <class T>
void Singleton<T>::init()
{
    static Singleton singleton;
    if (singleton.inited)
    {
#if QT_VERSION < 0x051400
        CreateInstanceFunction createFunction = (CreateInstanceFunction)Singleton::create.load();
        tptr.store(createFunction());
#else
        CreateInstanceFunction createFunction = (CreateInstanceFunction)Singleton::create.loadRelaxed();
        tptr.storeRelaxed(createFunction());
#endif
    }
}

template <class T>
Singleton<T>::Singleton()
{
    inited = true;
}

template <class T>
Singleton<T>::~Singleton()
{
    T* createdTptr = (T*)tptr.fetchAndStoreOrdered(nullptr);
    if (createdTptr)
    {
        delete createdTptr;
    }
#if QT_VERSION < 0x051400
    create.store(nullptr);
#else
    create.storeRelaxed(nullptr);
#endif
}

template<class T> QBasicAtomicPointer<void> Singleton<T>::create = Q_BASIC_ATOMIC_INITIALIZER(nullptr);
template<class T> QBasicAtomicInt Singleton<T>::flag = Q_BASIC_ATOMIC_INITIALIZER(CallOnce::CO_Request);
template<class T> QBasicAtomicPointer<void> Singleton<T>::tptr = Q_BASIC_ATOMIC_INITIALIZER(nullptr);

#endif // SINGLETON_H
