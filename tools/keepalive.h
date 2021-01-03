/**
--------------------------------------------------------------------------------
-   Module      :   keepalive.h
-   Description :   A wrapper to keep an app alive, written
-                   in C++ and Objective-C so made for the OSX platform.
-   Author      :   Tim Zaman, 18-FEB-2016
--------------------------------------------------------------------------------
*/

/*

Copyright (c) 2016 Tim Zaman

Permission to use, copy, modify, distribute, and sell this software
for any purpose is hereby granted without fee, provided
that (i) the above copyright notices and this permission notice appear in
all copies of the software and related documentation.

THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND,
EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY
WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.

*/

#ifndef KEEPALIVE_H
#define KEEPALIVE_H

class KeepAlive {
 public:
    KeepAlive(){
        KeepAliveMM();
    }

    ~KeepAlive(){
        KeepAliveDestructorMM();
    }

#ifdef __APPLE__
    void KeepAliveMM();
    void KeepAliveDestructorMM();
#else
    // Keep other platforms alive
#endif

};

#endif
