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
#include "../include/statemachine.h"
#include "../include/gui.h"
#include "../include/controller.h"
#include <string>
#include <SDL.h>

int gState = 0;
string gGame="";
bool gQuit=false;
bool gIgnore = false;

bool statemachine(int cmd)
{
    string execute;
    bool emuReturn;
    
    //printf("gState: %i   cmd: %i\n",gState,cmd);
    switch (cmd)
    {
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: 
        case SDLK_DOWN:
        case SDLK_RIGHT:
            switch (gState)
            {
                case STATE_ZERO:
                    if (gNumDesignatedControllers)
                    {
                        gState=STATE_PLAY;
                    }
                    else
                    {
                        gState=STATE_OFF;
                    }
                    break;
                case STATE_PLAY:
                    gState=STATE_SETUP;
                    break;
                case STATE_SETUP:
                    gState=STATE_OFF;
                    break;
                case STATE_OFF:
                    if (gDesignatedControllers[0].instance != -1)
                    {
                        gState=STATE_CONF0;
                    } 
                    else if (gDesignatedControllers[1].instance != -1)
                    {
                        gState=STATE_CONF1;
                    }
                    break;
                 case STATE_CONF0:
                    if (gDesignatedControllers[1].instance != -1)
                    {
                        gState=STATE_CONF1;
                    } 
                    else 
                    {
                        gState=STATE_LAUNCH;
                    }
                    break;
                case STATE_CONF1:
                    gState=STATE_LAUNCH;
                    break;
                case STATE_LAUNCH:
                    gState=STATE_PLAY;
                    break;
                default:
                    (void) 0;
                    break;
            }
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_UP:
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
        case SDLK_UP:
        case SDLK_LEFT:
            switch (gState)
            {
                case STATE_ZERO:
                    if (gNumDesignatedControllers)
                    {
                        gState=STATE_LAUNCH;;
                    }
                    else
                    {
                        gState=STATE_OFF;
                    }
                    break;
                case STATE_PLAY:
                    gState=STATE_LAUNCH;
                    break;
                case STATE_SETUP:
                    gState=STATE_PLAY;
                    break;
                case STATE_OFF:
                    if (gNumDesignatedControllers)
                    {
                        gState=STATE_SETUP;
                    }
                    break;
                 case STATE_CONF0:
                    gState=STATE_OFF;
                    break;
                case STATE_CONF1:
                    if (gDesignatedControllers[0].instance != -1)
                    {
                        gState=STATE_CONF0;
                    } 
                    else 
                    {
                        gState=STATE_OFF;
                    }
                    break;
                case STATE_LAUNCH:
                    if (gDesignatedControllers[1].instance != -1)
                    {
                        gState=STATE_CONF1;
                    } 
                    else if (gDesignatedControllers[0].instance != -1)
                    {
                        gState=STATE_CONF0;
                    }
                    else
                    {
                        gState=STATE_OFF;
                    }
                    break;
                default:
                    (void) 0;
                    break;
            }
            break;
        case SDL_CONTROLLER_BUTTON_A:
            switch (gState)
            {
                case STATE_ZERO:
                    
                    break;
                case STATE_PLAY:
                    gState=STATE_LAUNCH;
                    break;
                case STATE_SETUP:
                    
                    break;
                case STATE_OFF:
                    gQuit=true;
                    break;
                 case STATE_CONF0:
                    //ctrlConf(0);
                    break;
                case STATE_CONF1:
                    if (gDesignatedControllers[1].instance != -1)
                    {
                        gState=STATE_ZERO;
                    }
                    //ctrlConf(0);
                    break;
                case STATE_LAUNCH:
                    if (gGame != "")
                    {
                        #ifdef MEDNAFEN
                            int argc = 2;
                            
                            string str = "mednafen";
                            int n = str.length(); 
                            char arg1[n + 1]; 
                            strcpy(arg1, str.c_str()); 
                            
                            n = gGame.length(); 
                            char arg2[n + 1]; 
                            strcpy(arg2, gGame.c_str()); 
                            
                            char *argv[] ={arg1,arg2};
                            printf("arg1: %s arg2: %s \n",arg1,arg2);
                            mednafen_main(argc,argv);
                            gIgnore = true;
                            restoreSDL();
                        #else
                            execute = "mednafen \""+gGame+"\"";
                            emuReturn = system(execute.c_str());
                        #endif
                    } else
                    {
                        printf("no valid ROM found\n");
                    }
                    break;
                default:
                    (void) 0;
                    break;
            }
            break;
        default:
            (void) 0;
            break;
    }
    return 0;
}