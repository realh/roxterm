/*
    roxterm - VTE/GTK terminal emulator with tabs
    Copyright (C) 2004-2024 Tony Houghton <h@realh.co.uk>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#pragma once

#include <fstream>
#include <mutex>
#include <ostream>
#include <sstream>

namespace shim {

class EndLog {
private:
    bool nl;
public:
    EndLog(bool nl = true) : nl(nl) {}

    friend class LockedLog;
};

extern EndLog endlog;

class LockedLogOwner {
protected:
    virtual std::mutex &mutex() = 0;
    virtual std::ostream &output() = 0;
    virtual class ShimLog &self() = 0;
public:
    virtual ~LockedLogOwner() = default;

    friend class LockedLog;
};

class LockedLog {
private:
    LockedLogOwner &owner;
    std::stringstream ss;
public:
    LockedLog(LockedLogOwner &owner) : owner(owner) {}

    LockedLog(LockedLog &&src) : owner(src.owner), ss(std::move(src.ss)) {}

    template<class T> LockedLog operator<<(T a)
    {
        ss << a;
        return std::move(*this);
    }

    class ShimLog &operator<<(EndLog a);
};

class ShimLog : LockedLogOwner {
private:
    std::mutex mut;
    std::ofstream ostream{"/home/tony/.cache/roxterm/shimlog"};
protected:
    std::mutex &mutex()
    {
        return mut;
    }

    std::ostream &output()
    {
        return ostream;
    }

    class ShimLog &self()
    {
        return *this;
    }
public:
    template<class T> LockedLog operator<<(T a)
    {
        return LockedLog(*this) << a;
    }

    ShimLog &operator<<(EndLog a)
    {
        return LockedLog(*this) << a;
    }
};

extern ShimLog shimlog;

}