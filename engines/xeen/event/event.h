/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef XEEN_EVENT_H
#define XEEN_EVENT_H

#include "xeen/utility.h"
#include "xeen/ui/window.h"
#include "xeen/ui/basicwindows.h"

namespace XEEN
{
    namespace Event
    {
        typedef Common::List<Valid<Window> > WindowList;

        class Event : public Validateable, public GameHolder, public Common::NonCopyable
        {
            public:
                Event(Valid<Game> parent) : GameHolder(parent), _commandWindow(0), _characterWindow(0), _finished(false), _delete(false) { }

                virtual ~Event()
                {
                    removeCommandWindow();
                    removeCharacterWindow();
                }

                virtual void process() = 0;

                template <class FUNCTOR, typename ARGUMENT>
                void processWindows(ARGUMENT a)
                {
                    FUNCTOR f(a);

                    if(_commandWindow)
                    {
                        f(_commandWindow);
                    }

                    if(_characterWindow)
                    {
                        f(_characterWindow);
                    }

                    for(WindowList::iterator i = _windows.begin(); i != _windows.end() && !_finished; i ++)
                    {
                        f(*i);
                    }
                }

                const bool isFinished() const { return _finished; }
                const bool wantsDelete() const { return _delete; }
                void setFinished(bool finished, bool adelete) { _finished = finished; _delete = adelete; }
    
                virtual const Button* getCommandButtons() const { return 0; }
                virtual void handleAction(unsigned id) { }
    
                WindowList& getWindows() { return _windows; }
                void addWindow(Valid<Window> window) { _windows.push_back(window); window->show(); }
                void closeWindow() { _windows.pop_back(); }

                GameWindow* getCommandWindow() const { return _commandWindow; }
                CharacterWindow* getCharacterWindow() const { return _characterWindow; }

            protected:
                void setCommandWindow(Valid<GameWindow> window) { removeCommandWindow(); _commandWindow = window; window->show(); }
                void removeCommandWindow() { XEEN_DELETE(_commandWindow); }

                void setCharacterWindow(Valid<CharacterWindow> window) { removeCharacterWindow(); _characterWindow = window; window->show(); }
                void removeCharacterWindow() { XEEN_DELETE(_characterWindow); }


            private:
                WindowList _windows;
                XEEN::GameWindow* _commandWindow;
                XEEN::CharacterWindow* _characterWindow;

                bool _finished;
                bool _delete;
        };
    }
}

#endif // XEEN_EVENT_H
