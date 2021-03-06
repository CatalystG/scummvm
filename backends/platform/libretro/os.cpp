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

#define FORBIDDEN_SYMBOL_ALLOW_ALL

#include <unistd.h>
#include <sys/time.h>
#include <list>

#include "graphics/surface.h"
#include "backends/base-backend.h"
#include "common/events.h"
#include "audio/mixer_intern.h"
#include "backends/fs/posix/posix-fs-factory.h"
#include "backends/timer/default/default-timer.h"
#include "graphics/colormasks.h"
#include "graphics/palette.h"
#include "backends/saves/default/default-saves.h"
#if defined(_WIN32)
#include <direct.h>
#ifdef _XBOX
#include <xtl.h>
#else
#include <windows.h>
#endif
#elif defined(__CELLOS_LV2__)
#include <sys/sys_time.h>
#include <sys/timer.h>
#else
#include <time.h>
#endif

#include "libretro.h"

struct RetroPalette
{
    byte _colors[256 * 3];

    RetroPalette()
    {
        memset(_colors, 0, sizeof(_colors));
    }

    void set(const byte *colors, uint start, uint num)
    {
        memcpy(_colors + start * 3, colors, num * 3);
    }

    void get(byte* colors, uint start, uint num)
    {
        memcpy(colors, _colors + start * 3, num * 3);
    }

    void getColor(uint aIndex, byte& aR, byte& aG, byte& aB) const
    {
        if(aIndex < 256)
        {
            aR = _colors[aIndex * 3 + 0];
            aG = _colors[aIndex * 3 + 1];
            aB = _colors[aIndex * 3 + 2];
        }
    }
};


template<typename INPUT, typename OUTPUT>
static void blit(Graphics::Surface& aOut, const Graphics::Surface& aIn, int aX, int aY, const RetroPalette& aColors, uint32 aKeyColor)
{
    assert(sizeof(OUTPUT) == aOut.format.bytesPerPixel && sizeof(INPUT) == aIn.format.bytesPerPixel);

    for(int i = 0; i < aIn.h; i ++)
    {
        if((i + aY) < 0 || (i + aY) >= aOut.h)
           continue;

        INPUT* const in = (INPUT*)aIn.pixels + (i * aIn.w);
        OUTPUT* const out = (OUTPUT*)aOut.pixels + ((i + aY) * aOut.w);

        for(int j = 0; j < aIn.w; j ++)
        {
            if((j + aX) < 0 || (j + aX) >= aOut.w)
               continue;

            uint8 r, g, b;

            const INPUT val = in[j];
            if(val != aKeyColor)
            {
                if(aIn.format.bytesPerPixel == 1)
                    aColors.getColor(val, r, g, b);
                else
                    aIn.format.colorToRGB(in[j], r, g, b);

                out[j + aX] = aOut.format.RGBToColor(r, g, b);
            }
        }
    }
}

static void copyRectToSurface(Graphics::Surface& out, const void *buf, int pitch, int x, int y, int w, int h)
{
    const byte *src = (const byte *)buf;
    byte *dst = (byte *)out.pixels + y * out.pitch + x * out.format.bytesPerPixel;

    for (int i = 0; i < h; i++)
    {
        memcpy(dst, src, w * out.format.bytesPerPixel);
        src += pitch;
        dst += out.pitch;
    }
}

static Common::String s_systemDir;

class OSystem_RETRO : public EventsBaseBackend, public PaletteManager {
public:
    Graphics::Surface _screen;

    Graphics::Surface _gameScreen;
    RetroPalette _gamePalette;

    Graphics::Surface _overlay;
    bool _overlayVisible;

    Graphics::Surface _mouseImage;
    RetroPalette _mousePalette;
    bool _mousePaletteEnabled;
    bool _mouseVisible;
    int _mouseX;
    int _mouseY;
    int _mouseHotspotX;
    int _mouseHotspotY;
    int _mouseKeyColor;
    bool _mouseDontScale;
    bool _mouseButtons[2];
    bool _joypadmouseButtons[2];

    uint32 _startTime;
    uint32 _threadExitTime;

    std::list<Common::Event> _events;

    Audio::MixerImpl* _mixer;

    int _keyflags;

	OSystem_RETRO() :
	    _mousePaletteEnabled(false), _mouseVisible(false), _mouseX(0), _mouseY(0), _mouseHotspotX(0), _mouseHotspotY(0),
	    _mouseKeyColor(0), _mouseDontScale(false), _mixer(0), _startTime(0), _threadExitTime(10), _keyflags(0)
	{
        _fsFactory = new POSIXFilesystemFactory();
        memset(_mouseButtons, 0, sizeof(_mouseButtons));
        memset(_joypadmouseButtons, 0, sizeof(_joypadmouseButtons));

        _startTime = getMillis();

        if(s_systemDir.empty())
            s_systemDir = ".";
	}

	virtual ~OSystem_RETRO()
	{
	    _gameScreen.free();
	    _overlay.free();
	    _mouseImage.free();
	    _screen.free();

	    delete _mixer;
	}

	virtual void initBackend()
	{
	    _savefileManager = new DefaultSaveFileManager();
#ifdef FRONTEND_SUPPORTS_RGB565
       _overlay.create(640, 480, Graphics::PixelFormat(2, 5, 6, 5, 0, 11, 5, 0, 0));
#else
       _overlay.create(640, 480, Graphics::PixelFormat(2, 5, 5, 5, 1, 10, 5, 0, 15));
#endif
        _mixer = new Audio::MixerImpl(this, 44100);
        _timerManager = new DefaultTimerManager();

        _mixer->setReady(true);

        BaseBackend::initBackend();
	}

	virtual bool hasFeature(Feature f)
	{
    	return (f == OSystem::kFeatureCursorPalette);
	}

	virtual void setFeatureState(Feature f, bool enable)
	{
        if (f == kFeatureCursorPalette)
            _mousePaletteEnabled = enable;
	}

	virtual bool getFeatureState(Feature f)
	{
        return (f == kFeatureCursorPalette) ? _mousePaletteEnabled : false;
	}

	virtual const GraphicsMode *getSupportedGraphicsModes() const
	{
        static const OSystem::GraphicsMode s_noGraphicsModes[] = { {0, 0, 0} };
        return s_noGraphicsModes;
	}

	virtual int getDefaultGraphicsMode() const
	{
	    return 0;
	}

	virtual bool setGraphicsMode(int mode)
	{
        return true;
	}

	virtual int getGraphicsMode() const
	{
	    return 0;
	}

	virtual void initSize(uint width, uint height, const Graphics::PixelFormat *format)
	{
        _gameScreen.create(width, height, format ? *format : Graphics::PixelFormat::createFormatCLUT8());
	}

	virtual int16 getHeight()
	{
	    return _gameScreen.h;
	}

	virtual int16 getWidth()
	{
	    return _gameScreen.w;
	}

	virtual Graphics::PixelFormat getScreenFormat() const
	{
	    return _gameScreen.format;
	}

	virtual Common::List<Graphics::PixelFormat> getSupportedFormats() const
	{
        Common::List<Graphics::PixelFormat> result;

        /* RGBA8888 */
        result.push_back(Graphics::PixelFormat(4, 8, 8, 8, 8, 24, 16, 8, 0));

#ifdef FRONTEND_SUPPORTS_RGB565
        /* RGB565 - overlay */
        result.push_back(Graphics::PixelFormat(2, 5, 6, 5, 0, 11, 5, 0, 0));
#endif
        /* RGB555 - fmtowns */
        result.push_back(Graphics::PixelFormat(2, 5, 5, 5, 1, 10, 5, 0, 15));

        /* Palette - most games */
        result.push_back(Graphics::PixelFormat::createFormatCLUT8());
        return result;
	}



	virtual PaletteManager *getPaletteManager() { return this; }
protected:
	// PaletteManager API
	virtual void setPalette(const byte *colors, uint start, uint num)
	{
        _gamePalette.set(colors, start, num);
	}

	virtual void grabPalette(byte *colors, uint start, uint num)
	{
        _gamePalette.get(colors, start, num);
	}



public:
	virtual void copyRectToScreen(const void *buf, int pitch, int x, int y, int w, int h)
	{
	    copyRectToSurface(_gameScreen, buf, pitch, x, y, w, h);
	}

	virtual void updateScreen()
	{
	    /* EMPTY */
	}

	virtual Graphics::Surface *lockScreen()
	{
        return &_gameScreen;
	}

	virtual void unlockScreen()
	{
	    /* EMPTY */
	}

	virtual void setShakePos(int shakeOffset)
	{
	    // TODO
	}

	virtual void showOverlay()
	{
	    _overlayVisible = true;
	}

	virtual void hideOverlay()
	{
	    _overlayVisible = false;
	}

	virtual void clearOverlay()
	{
        _overlay.fillRect(Common::Rect(_overlay.w, _overlay.h), 0);
	}

	virtual void grabOverlay(void *buf, int pitch)
	{
        const byte *src = (byte *)_overlay.pixels;
        byte *dst = (byte *)buf;

        for (int i = 0; i < 480; i++, dst += pitch, src += 640 * 2)
           memcpy(dst, src, 640 * 2);
	}

	virtual void copyRectToOverlay(const void *buf, int pitch, int x, int y, int w, int h)
	{
	    copyRectToSurface(_overlay, buf, pitch, x, y, w, h);
	}

	virtual int16 getOverlayHeight()
	{
	    return _overlay.h;
	}

	virtual int16 getOverlayWidth()
	{
	    return _overlay.w;
	}

	virtual Graphics::PixelFormat getOverlayFormat() const
	{
	    return _overlay.format;
	}



	virtual bool showMouse(bool visible)
	{
        const bool wasVisible = _mouseVisible;
        _mouseVisible = visible;
        return wasVisible;
	}

	virtual void warpMouse(int x, int y)
	{
        _mouseX = x;
        _mouseY = y;
	}

	virtual void setMouseCursor(const void *buf, uint w, uint h, int hotspotX, int hotspotY, uint32 keycolor = 255, bool dontScale = false, const Graphics::PixelFormat *format = NULL)
	{
	    const Graphics::PixelFormat mformat = format ? *format : Graphics::PixelFormat::createFormatCLUT8();

        if(_mouseImage.w != w || _mouseImage.h != h || _mouseImage.format != mformat)
        {
            _mouseImage.create(w, h, mformat);
        }

    	memcpy(_mouseImage.pixels, buf, h * _mouseImage.pitch);

        _mouseHotspotX = hotspotX;
        _mouseHotspotY = hotspotY;
        _mouseKeyColor = keycolor;
        _mouseDontScale = dontScale;
	}

	virtual void setCursorPalette(const byte *colors, uint start, uint num)
	{
        _mousePalette.set(colors, start, num);
        _mousePaletteEnabled = true;
	}

    bool retroCheckThread(uint32 offset = 0)
    {
        if(_threadExitTime <= (getMillis() + offset))
        {
            extern void retro_leave_thread();
            retro_leave_thread();

            _threadExitTime = getMillis() + 10;
            return true;
        }

        return false;
    }

	virtual bool pollEvent(Common::Event &event)
	{
	    retroCheckThread();

        ((DefaultTimerManager*)_timerManager)->handler();


        if(!_events.empty())
        {
            event = _events.front();
            _events.pop_front();
            return true;
        }

        return false;
	}

	virtual uint32 getMillis()
	{
#if defined(GEKKO)
   return (ticks_to_microsecs(gettime()) / 1000.0) - _startTime;
#elif defined(__CELLOS_LV2__)
   return (sys_time_get_system_time() / 1000.0) - _startTime;
#else
        struct timeval t;
        gettimeofday(&t, 0);

        return ((t.tv_sec * 1000) + (t.tv_usec / 1000)) - _startTime;
#endif
	}

	virtual void delayMillis(uint msecs)
	{
		if(!retroCheckThread(msecs))
      {
#if defined(_WIN32)
         Sleep(1000 * msecs);
#elif defined(__CELLOS_LV2__)
         sys_timer_usleep(1000 * msecs);
#else
         usleep(1000 * msecs);
#endif
      }
	}


	virtual MutexRef createMutex(void)
	{
	    return MutexRef();
	}

	virtual void lockMutex(MutexRef mutex)
	{
	    /* EMPTY */
	}

	virtual void unlockMutex(MutexRef mutex)
	{
	    /* EMPTY */
	}

	virtual void deleteMutex(MutexRef mutex)
	{
	    /* EMPTY */
	}

	virtual void quit()
	{
	    // TODO:
	}

	virtual void addSysArchivesToSearchSet(Common::SearchSet &s, int priority = 0)
	{
	    // TODO: NOTHING?
	}

	virtual void getTimeAndDate(TimeDate &t) const
	{
        time_t curTime = time(0);
        struct tm lt = *localtime(&curTime);
        t.tm_sec = lt.tm_sec;
        t.tm_min = lt.tm_min;
        t.tm_hour = lt.tm_hour;
        t.tm_mday = lt.tm_mday;
        t.tm_mon = lt.tm_mon;
        t.tm_year = lt.tm_year;
        t.tm_wday = lt.tm_wday;
	}

	virtual Audio::Mixer *getMixer()
	{
	    return _mixer;
	}

	virtual Common::String getDefaultConfigFileName()
	{
	    return s_systemDir + "/scummvm.ini";
	}

	virtual void logMessage(LogMessageType::Type type, const char *message)
	{
	    printf("%s\n", message);
	}


	//

	const Graphics::Surface& getScreen()
	{
        const Graphics::Surface& srcSurface = (_overlayVisible) ? _overlay : _gameScreen;

        if(srcSurface.w != _screen.w || srcSurface.h != _screen.h)
        {
#ifdef FRONTEND_SUPPORTS_RGB565
            _screen.create(srcSurface.w, srcSurface.h, Graphics::PixelFormat(2, 5, 6, 5, 0, 11, 5, 0, 0));
#else
            _screen.create(srcSurface.w, srcSurface.h, Graphics::PixelFormat(2, 5, 5, 5, 1, 10, 5, 0, 15));
#endif
        }

        if(srcSurface.w && srcSurface.h)
        {
            switch(srcSurface.format.bytesPerPixel)
            {
                case 1: blit<uint8, uint16>(_screen, srcSurface, 0, 0, _gamePalette, 0xFFFFFFFF); break;
                case 2: blit<uint16, uint16>(_screen, srcSurface, 0, 0, _gamePalette, 0xFFFFFFFF); break;
                case 3: blit<uint8, uint16>(_screen, srcSurface, 0, 0, _gamePalette, 0xFFFFFFFF); break;
                case 4: blit<uint32, uint16>(_screen, srcSurface, 0, 0, _gamePalette, 0xFFFFFFFF); break;
            }
        }

        // Draw Mouse
        if(_mouseVisible && _mouseImage.w && _mouseImage.h)
        {
            const int x = _mouseX - _mouseHotspotX;
            const int y = _mouseY - _mouseHotspotY;

            blit<uint8, uint16>(_screen, _mouseImage, x, y, _mousePaletteEnabled ? _mousePalette : _gamePalette, _mouseKeyColor);
        }

        return _screen;
	}

#define ANALOG_VALUE_X_ADD 8
#define ANALOG_VALUE_Y_ADD 8

	void processMouse(retro_input_state_t aCallback)
    {
        int16_t joy_x, joy_y, x, y;
        bool do_joystick, down;

        static const uint32_t retroButtons[2] = {RETRO_DEVICE_ID_MOUSE_LEFT, RETRO_DEVICE_ID_MOUSE_RIGHT};
        static const Common::EventType eventID[2][2] =
        {
            {Common::EVENT_LBUTTONDOWN, Common::EVENT_LBUTTONUP},
            {Common::EVENT_RBUTTONDOWN, Common::EVENT_RBUTTONUP}
        };

        down = false;
        do_joystick = false;
        x = aCallback(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
        y = aCallback(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);
        joy_x = aCallback(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
        joy_y = aCallback(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);

        if (joy_x > 0)
        {
           _mouseX += ANALOG_VALUE_X_ADD;
           _mouseX = (_mouseX < 0) ? 0 : _mouseX;
           _mouseX = (_mouseX >= _screen.w) ? _screen.w : _mouseX;
            do_joystick = true;
        }
        else if (joy_x < 0)
        {
           _mouseX -= ANALOG_VALUE_X_ADD;
           _mouseX = (_mouseX < 0) ? 0 : _mouseX;
           _mouseX = (_mouseX >= _screen.w) ? _screen.w : _mouseX;
            do_joystick = true;
        }

        if (joy_y > 0)
        {
            _mouseY += ANALOG_VALUE_Y_ADD; 
            _mouseY = (_mouseY < 0) ? 0 : _mouseY;
            _mouseY = (_mouseY >= _screen.h) ? _screen.h : _mouseY;
            do_joystick = true;
        }
        else if (joy_y < 0)
        {
            _mouseY -= ANALOG_VALUE_Y_ADD; 
            _mouseY = (_mouseY < 0) ? 0 : _mouseY;
            _mouseY = (_mouseY >= _screen.h) ? _screen.h : _mouseY;
            do_joystick = true;
        }

        {
           if (aCallback(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT))
           {
              _mouseX -= ANALOG_VALUE_X_ADD >> 1;
              _mouseX = (_mouseX < 0) ? 0 : _mouseX;
              _mouseX = (_mouseX >= _screen.w) ? _screen.w : _mouseX;
              do_joystick = true;
           }

           if (aCallback(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT))
           {
              _mouseX += ANALOG_VALUE_X_ADD >> 1;
              _mouseX = (_mouseX < 0) ? 0 : _mouseX;
              _mouseX = (_mouseX >= _screen.w) ? _screen.w : _mouseX;
              do_joystick = true;
           }

           if (aCallback(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP))
           {
              _mouseY -= ANALOG_VALUE_Y_ADD >> 1; 
              _mouseY = (_mouseY < 0) ? 0 : _mouseY;
              _mouseY = (_mouseY >= _screen.h) ? _screen.h : _mouseY;
              do_joystick = true;
           }

           if (aCallback(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN))
           {
              _mouseY += ANALOG_VALUE_Y_ADD >> 1; 
              _mouseY = (_mouseY < 0) ? 0 : _mouseY;
              _mouseY = (_mouseY >= _screen.h) ? _screen.h : _mouseY;
              do_joystick = true;
           }
        }

        if (aCallback(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START))
        {
            Common::Event ev;
            ev.type = Common::EVENT_MAINMENU;
            _events.push_back(ev);
        }

        if (do_joystick)
        {
            Common::Event ev;
            ev.type = Common::EVENT_MOUSEMOVE;
            ev.mouse.x = _mouseX;
            ev.mouse.y = _mouseY;
            _events.push_back(ev);
        }

        down = aCallback(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);

        if(down != _joypadmouseButtons[0])
        {
           _joypadmouseButtons[0] = down;

           Common::Event ev;
           ev.type = eventID[0][down ? 0 : 1];
           ev.mouse.x = _mouseX;
           ev.mouse.y = _mouseY;
           _events.push_back(ev);
        }

        down = aCallback(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);

        if(down != _joypadmouseButtons[0])
        {
           _joypadmouseButtons[1] = down;

           Common::Event ev;
           ev.type = eventID[1][down ? 0 : 1];
           ev.mouse.x = _mouseX;
           ev.mouse.y = _mouseY;
           _events.push_back(ev);
        }

        if(x || y)
        {
            _mouseX += x;
            _mouseX = (_mouseX < 0) ? 0 : _mouseX;
            _mouseX = (_mouseX >= _screen.w) ? _screen.w : _mouseX;

            _mouseY += y;
            _mouseY = (_mouseY < 0) ? 0 : _mouseY;
            _mouseY = (_mouseY >= _screen.h) ? _screen.h : _mouseY;

            Common::Event ev;
            ev.type = Common::EVENT_MOUSEMOVE;
            ev.mouse.x = _mouseX;
            ev.mouse.y = _mouseY;
            _events.push_back(ev);
        }


        for(int i = 0; i < 2; i ++)
        {
           Common::Event ev;
            bool down = aCallback(0, RETRO_DEVICE_MOUSE, 0, retroButtons[i]);
            if(down != _mouseButtons[i])
            {
                _mouseButtons[i] = down;

                ev.type = eventID[i][down ? 0 : 1];
                ev.mouse.x = _mouseX;
                ev.mouse.y = _mouseY;
                _events.push_back(ev);
            }

        }
    }

    void processKeyEvent(bool down, unsigned keycode, uint32_t character, uint16_t key_modifiers)
    {
        _keyflags = 0;
        _keyflags |= (key_modifiers & RETROKMOD_CTRL) ? Common::KBD_CTRL : 0;
        _keyflags |= (key_modifiers & RETROKMOD_ALT) ? Common::KBD_ALT : 0;
        _keyflags |= (key_modifiers & RETROKMOD_SHIFT) ? Common::KBD_SHIFT : 0;
        _keyflags |= (key_modifiers & RETROKMOD_META) ? Common::KBD_META : 0;
        _keyflags |= (key_modifiers & RETROKMOD_CAPSLOCK) ? Common::KBD_CAPS : 0;
        _keyflags |= (key_modifiers & RETROKMOD_NUMLOCK) ? Common::KBD_NUM : 0;
        _keyflags |= (key_modifiers & RETROKMOD_SCROLLOCK) ? Common::KBD_SCRL : 0;

        Common::Event ev;
        ev.type = down ? Common::EVENT_KEYDOWN : Common::EVENT_KEYUP;
        ev.kbd.keycode = (Common::KeyCode)keycode;
        ev.kbd.flags = _keyflags;
        ev.kbd.ascii = character & 0x7F;
        _events.push_back(ev);
    }

    void postQuit()
    {
        Common::Event ev;
        ev.type = Common::EVENT_QUIT;
        _events.push_back(ev);
    }
};

OSystem* retroBuildOS()
{
    return new OSystem_RETRO();
}

const Graphics::Surface& getScreen()
{
    return ((OSystem_RETRO*)g_system)->getScreen();
}

void retroProcessMouse(retro_input_state_t aCallback)
{
    ((OSystem_RETRO*)g_system)->processMouse(aCallback);
}

void retroPostQuit()
{
    ((OSystem_RETRO*)g_system)->postQuit();
}

void retroSetSystemDir(const char* aPath)
{
    s_systemDir = Common::String(aPath ? aPath : ".");
}

void retroKeyEvent(bool down, unsigned keycode, uint32_t character, uint16_t key_modifiers)
{
    ((OSystem_RETRO*)g_system)->processKeyEvent(down, keycode, character, key_modifiers);
}
