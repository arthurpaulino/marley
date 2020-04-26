/* Marley Copyright (c) 2020 Marley Development Team 
   https://github.com/beaumanvienna/marley

   Permission is hereby granted, free of charge, to any person
   obtaining a copy of this software and associated documentation files
   (the "Software"), to deal in the Software without restriction,
   including without limitation the rights to use, copy, modify, merge,
   publish, distribute, sublicense, and/or sell copies of the Software,
   and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#include "../include/marley.h"
#include "../include/gui.h"
#include "../include/controller.h"
#include "../include/statemachine.h"
#include "../include/emu.h"

#define DOLPHIN_HEADRES_FOR_WIIMOTE 1
#include "../include/wii.h"

bool initWii(void)
{
    bool ok = true;
    
    printf("InitWii\n");
    
    UICommon::SetUserDirectory("");
    UICommon::CreateDirectories();
    UICommon::Init();

    Wiimote::Initialize(Wiimote::InitializeMode::DO_NOT_WAIT_FOR_WIIMOTES);

    return ok;
}

bool mainLoopWii(void)
{
    Core::HostDispatchJobs();
}

bool shutdownWii(void)
{
    Core::Stop();
    Core::Shutdown();
    UICommon::Shutdown();
}

