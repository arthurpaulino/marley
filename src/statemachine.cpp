/* Marley Copyright (c) 2020-2021 Marley Development Team 
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

#include <string>
#include <SDL.h>

#include "../include/marley.h"
#include "../include/statemachine.h"
#include "../include/gui.h"
#include "../include/controller.h"
#include "../include/emu.h"
#include <algorithm>
#include <X11/Xlib.h>
#include <fstream>

#include <sys/stat.h>
#include <unistd.h>
#include <string>
#include <dirent.h>
#include <errno.h>
#include <SDL_syswm.h>

int gState = STATE_CONFIG;
int gCurrentGame;
std::vector<string> gGame;
bool gQuit=false;
bool gSetupIsRunning=false;
bool gTextInput=false;
bool gTextInputForGamingFolder = false;
bool gTextInputForFirmwareFolder = false;
string gText;
string gTextForGamingFolder;
string gTextForFirmwareFolder;
bool gControllerConf = false;
int gControllerConfNum=NO_CONTROLLER;
int confState;
string gConfText,gConfText2;
int gControllerButton[STATE_CONF_MAX];
int xCount,yCount,xValue,yValue;
int gHat[4],gHatValue[4];
int hatIterator;
int gAxis[4];
bool gAxisValue[4];
int axisIterator;
int secondRun;
int secondRunHat;
int secondRunValue;
bool launch_request_from_screen_manager;
bool restart_screen_manager;
string game_screen_manager;

extern Display* XDisplay;
extern Window Xwindow;
extern int delay_after_shutdown;
extern bool marley_wiimote;
extern bool stopSearching;
extern bool shutdown_now;
extern bool pcsx2_window_tear_down;
extern int pcsx2_window_tear_down_auto_request;
bool checkAxis(int cmd);
bool checkTrigger(int cmd);
void initOpenGL(void);
void setAppIcon(void);
void hide_or_show_cursor_X11(bool hide);
std::ifstream::pos_type filesize(const char* filename);

void startControllerConf(int controllerNum)
{
    for (int i=0;i<STATE_CONF_MAX;i++)
    {
        gControllerButton[i]=STATE_CONF_SKIP_ITEM;
    }

    for (int i = 0; i < 4;i++)
    {
        gHat[i] = -1;
        gHatValue[i] = -1;
        gAxis[i] = -1;
        gAxisValue[i] = false;
    }
    hatIterator = 0;
    axisIterator = 0;
    secondRun = -1;
    secondRunHat = -1;
    secondRunValue = -1;

    gControllerConf=true;
    gControllerConfNum=controllerNum;
    confState = STATE_CONF_BUTTON_DPAD_UP;
}

void resetStatemachine(void)
{
    gControllerConf = false;
    gControllerConfNum=NO_CONTROLLER;
}

bool exists(const char *fileName);
bool copyFile(const char *SRC, const char* DEST);
int mdf2iso_main (int argc, char **argv);
void create_cue_file(string filename)
{
    // A few words on how Marley is handling cue files: Cue files are
    // detected in emu.cpp and added to the list of available games. That
    // is, if the FILE reference in the cue file points to a valid file. In
    // this case, only the cue file will show up in the game list. The 
    // "bin" or "mdf" files are removed so that this function here does not
    // need to be called. "bin" and "mdf" files only make it on the list of
    // available games if they are not listed in any valid cue file.
    // This here function creates a cue file. If an invalid cue file 
    // exists, meaning the cue file's reference is broken,
    // create_cue_file creates a backup. The original cue file is overwritten.
    // If only an mdf file exists, a cue and a bin file are created.
    
    string ext = filename.substr(filename.find_last_of(".") + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(),
      [](unsigned char c){ return std::tolower(c); });
    string bin_filename = filename.substr(0,filename.find_last_of(".")) + ".bin";
    string cue_filename = filename.substr(0,filename.find_last_of(".")) + ".cue";
    string cue_filename_backup = cue_filename + ".backup";
    if (ext.find("bin") != string::npos) 
    {
        // If the cue file already exists, it has in incorrect file reference in it
        // such as FILE "game.BIN" when the actual file is called "game.bin".
        // A back up is created of the original file
        if (exists(cue_filename.c_str())) copyFile(cue_filename.c_str(),cue_filename_backup.c_str());
        
        std::ofstream cue_file;

        cue_file.open(cue_filename.c_str(), std::ios_base::trunc); 
        if(cue_file) 
        {
            cue_file << "FILE \"";
            if (filename.find_last_of("/") != string::npos)
            {
                cue_file << filename.substr(filename.find_last_of("/")+1);
            }
            else
            {
                cue_file << filename;
            }
            cue_file << "\" BINARY\n";
            cue_file << "  TRACK 01 MODE2/2352\n";
            cue_file << "    INDEX 01 00:00:00";
            cue_file.close();
            // replace bin file
        }
        else
        {
            printf("Could not create cue file for %s\n",filename.c_str());
        }
    }
    else if ((ext.find("mdf") != string::npos) && (!exists(bin_filename.c_str())))
    {
        string bin_filename_no_path = bin_filename;
        if (bin_filename.find("/") != string::npos)
        {
            bin_filename_no_path = bin_filename.substr(bin_filename.find_last_of("/") + 1);
        }
        char *argv[4]; 
        char arg1[10] = "mdf2iso"; 
        char arg2[10] = "--cue";
        char arg3[1024];
        char arg4[1024];
        strcpy(arg3, filename.c_str());  
        strcpy(arg4, bin_filename_no_path.c_str());

        argv[0] = arg1;
        argv[1] = arg2;
        argv[2] = arg3;
        argv[3] = arg4;
        
        mdf2iso_main (4, argv);
    }
    gGame[gCurrentGame] = cue_filename;
}

#define BUFSIZE 1024
enum emulator_target
{
    unknown,
    mednafen,
    dolphin,
    mupen64plus,
    ppsspp,
    pcsx2
};
emulator_target getEmulatorTarget(string filename)
{
    string cmd = "file -b \"" + filename + "\"";
    string file_type;
    char buf[BUFSIZE];
    FILE *fp;
    bool ok = false;
    emulator_target emu = unknown;
    
    if ((fp = popen(cmd.c_str(), "r")) == nullptr) 
    {
        printf("Error opening pipe for command %s\n",cmd.c_str());
    }
    else
    {
        if (fgets(buf, BUFSIZE, fp) != nullptr) 
        {
            file_type = buf;
            ok = true;
        }
        
        if(pclose(fp))  
        {
            printf("Command '%s' not found or exited with error status\n",cmd.c_str());
            ok = false;
        }
    }
    
    if (ok)
    {
        
        std::transform(file_type.begin(), file_type.end(), file_type.begin(),
            [](unsigned char c){ return std::tolower(c); });
        
        if ((file_type.find("nintendo wii") != string::npos) || (file_type.find("nintendo gamecube") != string::npos))
        {
            emu = dolphin; 
            printf("dolphin ");
        }
        else if (file_type.find("game boy") != string::npos)
        {
            emu = mednafen;
            printf("mednafen ");
        }
        else if ((file_type.find("sega mega drive") != string::npos) || (file_type.find("genesis") != string::npos))
        {
            emu = mednafen;
            printf("mednafen ");
        }
        else if (file_type.find("sega saturn") != string::npos)
        {
            create_cue_file(filename);
            emu = mednafen;
            printf("mednafen ");
        }
        else if (file_type.find("nes rom") != string::npos)
        {
            emu = mednafen;
            printf("mednafen ");
        }
        else if (file_type.find("nintendo 64") != string::npos)
        {
            emu = mupen64plus;
            printf("mupen64plus ");
        }
        else
        {
            long fsize = filesize(filename.c_str());
            if (fsize < 52428800) // less than 50MB must be either mednafen or mupen64plus
            {
                string ext = filename.substr(filename.find_last_of(".") + 1);
                if (ext.find("64") != string::npos) 
                {
                    emu = mupen64plus;
                }
                else
                {
                    emu = mednafen;
                }
            }
            else
            {
                if (file_type.find("iso 9660 cd-rom filesystem data") != string::npos)
                {
                    emu = ppsspp;
                    printf("ppsspp ");
                }
                else 
                {
                    emu = pcsx2;
                    printf("pcsx2 ");
                }
            }
        }
    }

    return emu;
}

void shutdown_computer(void)
{
    string cmd = "shutdown now";
    FILE *fp;
    
    if ((fp = popen(cmd.c_str(), "r")) == nullptr) 
    {
        printf("Error opening pipe for command %s\n",cmd.c_str());
    }
}

void tear_down_and_create_new_window(void)
{
    Uint32 window_flags = SDL_GetWindowFlags(gWindow);
    SDL_GetWindowSize(gWindow,&window_width,&window_height);
    SDL_GetWindowPosition(gWindow,&window_x,&window_y);
    
    freeTextures();
    SDL_DestroyRenderer( gRenderer );
    SDL_DestroyWindow(gWindow);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    
    if( SDL_Init( SDL_INIT_VIDEO ) < 0 )
    {
        printf( "SDL could not initialize! SDL Error: %s\n", SDL_GetError() );
    }
    
    string str = "marley ";
    str += PACKAGE_VERSION;

    gWindow = SDL_CreateWindow( str.c_str(), 
                            window_x,
                            window_y,
                            window_width, 
                            window_height, 
                            window_flags );

    setAppIcon(); 
    SDL_ShowCursor(SDL_DISABLE);
}

void launch_emulator(void)
{
    if (gGame[gCurrentGame] != "")
    {
        int argc;
        char *argv[10]; 
        
        char arg1[1024]; 
        char arg2[1024];
        char arg3[1024]; 
        char arg4[1024]; 
        char arg5[1024]; 
        char arg6[1024]; 
        char arg7[1024]; 
        char arg8[1024]; 
        char arg9[1024]; 
        char arg10[1024]; 
        
        int n;
        string str;
        Uint32 window_flags;
        window_flags=SDL_GetWindowFlags(gWindow);
        if (!(window_flags & SDL_WINDOW_FULLSCREEN_DESKTOP) && !(window_flags & SDL_WINDOW_FULLSCREEN))
        {
            SDL_GetWindowSize(gWindow,&window_width,&window_height);
            SDL_GetWindowPosition(gWindow,&window_x,&window_y);
        }
        
        argc = 2;
        emulator_target emulatorTarget;
        if (launch_request_from_screen_manager)
          emulatorTarget = getEmulatorTarget(game_screen_manager);
        else
          emulatorTarget = getEmulatorTarget(gGame[gCurrentGame]);
        switch((int)emulatorTarget)
        {
#ifdef PCSX2
            case pcsx2:
            
                if (pcsx2_window_tear_down) tear_down_and_create_new_window();
                pcsx2_window_tear_down_auto_request = 0;
                str = "pcsx2";
                strcpy(arg1, str.c_str()); 
                
                   str = "--nogui";
                strcpy(arg2, str.c_str());

                str = "--fullboot";
                strcpy(arg3, str.c_str());
                
                if (launch_request_from_screen_manager)
                  str = game_screen_manager;
                else
                  str = gGame[gCurrentGame];
                strcpy(arg4, str.c_str());

                argv[0] = arg1;
                argv[1] = arg2;
                argv[2] = arg3;
                argv[3] = arg4;

                argc = 4;

                SDL_SysWMinfo sdlWindowInfo;
                SDL_VERSION(&sdlWindowInfo.version);
                if(SDL_GetWindowWMInfo(gWindow, &sdlWindowInfo))
                {
                    if(sdlWindowInfo.subsystem == SDL_SYSWM_X11) 
                    {
                        Xwindow      = sdlWindowInfo.info.x11.window;
                        XDisplay     = sdlWindowInfo.info.x11.display;

                    }
                    pcsx2_main(argc,argv);
                    if (pcsx2_window_tear_down_auto_request == 1)
                    {
                        // try again with window tear down
                        pcsx2_window_tear_down = true;
                        tear_down_and_create_new_window();
                        str = "pcsx2";
                        strcpy(arg1, str.c_str()); 
                        
                        str = "--nogui";
                        strcpy(arg2, str.c_str());

                        str = "--fullboot";
                        strcpy(arg3, str.c_str());
                        
                        if (launch_request_from_screen_manager)
                          str = game_screen_manager;
                        else
                          str = gGame[gCurrentGame];
                        strcpy(arg4, str.c_str());

                        argv[0] = arg1;
                        argv[1] = arg2;
                        argv[2] = arg3;
                        argv[3] = arg4;

                        argc = 4;

                        SDL_SysWMinfo sdlWindowInfo;
                        SDL_VERSION(&sdlWindowInfo.version);
                        if(SDL_GetWindowWMInfo(gWindow, &sdlWindowInfo))
                        {
                            if(sdlWindowInfo.subsystem == SDL_SYSWM_X11) 
                            {
                                Xwindow      = sdlWindowInfo.info.x11.window;
                                XDisplay     = sdlWindowInfo.info.x11.display;

                            }
                            pcsx2_main(argc,argv);
                            restartGUI();
                        } 
                        else
                        {
                            printf("jc SDL_GetWindowWMInfo(gWindow, &sdlWindowInfo) failed\n");
                        }
                        
                        
                        
                        
                    }
                    restartGUI();
                } 
                else
                {
                    printf("jc SDL_GetWindowWMInfo(gWindow, &sdlWindowInfo) failed\n");
                }
                

                break;
#endif


#ifdef MUPEN64PLUS
            case mupen64plus:
                
                str = "mupen64plus";
                strcpy(arg1, str.c_str()); 
                
                if (launch_request_from_screen_manager)
                  str = game_screen_manager;
                else
                  str = gGame[gCurrentGame];
                strcpy(arg2, str.c_str()); 
                
                argv[0] = arg1;
                argv[1] = arg2;
                printf("arg1: %s arg2: %s \n",arg1,arg2);
                mupen64plus_main(argc,argv);
                restoreSDL();
                break;
#endif

#ifdef PPSSPP
        
            case ppsspp:
            
                str = "ppsspp";
                strcpy(arg1, str.c_str()); 
                
                if (launch_request_from_screen_manager)
                  str = game_screen_manager;
                else
                  str = gGame[gCurrentGame];
                strcpy(arg2, str.c_str()); 
                
                argv[0] = arg1;
                argv[1] = arg2;
                printf("arg1: %s arg2: %s \n",arg1,arg2);
                ppsspp_main(argc,argv);
                
                restoreSDL();
                break;
#endif

#ifdef DOLPHIN
        
            case dolphin:
                str = "dolphin-emu";
                strcpy(arg1, str.c_str()); 
                
                if (launch_request_from_screen_manager)
                  str = game_screen_manager;
                else
                  str = gGame[gCurrentGame];
                strcpy(arg2, str.c_str()); 
                
                argv[0] = arg1;
                argv[1] = arg2;
                printf("arg1: %s arg2: %s \n",arg1,arg2);
                marley_wiimote = false;
                dolphin_main(argc,argv);
                marley_wiimote = true;
                delay_after_shutdown = 10;
                restoreSDL();
                break;
#endif
        
        
#ifdef MEDNAFEN
            case mednafen:
                str = "mednafen"; 
                strcpy(arg1, str.c_str()); 
                
                if (launch_request_from_screen_manager)
                  str = game_screen_manager;
                else
                  str = gGame[gCurrentGame];
                strcpy(arg2, str.c_str()); 
                
                argv[0] = arg1;
                argv[1] = arg2;
                printf("arg1: %s arg2: %s \n",arg1,arg2);
                mednafen_main(argc,argv);
                restoreSDL();
                break;
#endif
            default:
                (void) 0;
                break;
        }
    }
}

void statemachine(int cmd)
{
    string execute;
    bool emuReturn = false;
    
    int screen_man_argc;
    char *screen_man_argv[1];
    char arg1[1024];
    int n;
    string str;
    
    if (!gControllerConf)
    {
        switch (cmd)
        {
            case SDL_CONTROLLER_BUTTON_GUIDE:
                if (gState == STATE_OFF)
                {
                    gQuit=true;
                }
                else
                {
                    resetStatemachine();
                }
                break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: 
            case SDLK_DOWN:
            case SDLK_RIGHT:
                switch (gState)
                {
                    case STATE_ZERO:
                        if (gNumDesignatedControllers)
                        {
                            if (gGamesFound)
                            {
                                gState=STATE_PLAY;
                            }
                            else
                            {
                                gState=STATE_SETUP;
                            }
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
                        gState=STATE_CONFIG;
                        break;
                    case STATE_CONFIG:
                        gState=STATE_OFF;
                        break;
                    case STATE_SHUTDOWN:
                    case STATE_OFF:
                        if (gGamesFound)
                        {
                            gState=STATE_LAUNCH;
                        }
                        else
                        {
                            gState=STATE_SETUP;
                        }
                        break;
                     case STATE_CONF0:
                        if (gDesignatedControllers[1].numberOfDevices != 0)
                        {
                            gState=STATE_CONF1;
                        } 
                        else 
                        {
                            gState=STATE_FLR_GAMES;
                        }
                        break;
                    case STATE_CONF1:
                        gState=STATE_FLR_GAMES;
                        break;
                    case STATE_FLR_GAMES:
                        gState=STATE_FLR_FW;
                        break;
                    case STATE_LAUNCH:
                        if (gCurrentGame == (gGame.size()-1))
                        {
                            gState=STATE_PLAY;
                        }
                        else
                        {
                            gCurrentGame++;
                        }
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
                            if (gGamesFound)
                            {
                                gState=STATE_LAUNCH;
                            }
                            else
                            {
                                gState=STATE_OFF;
                            }
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
                        if (gGamesFound)
                        {
                            gState=STATE_PLAY;
                        }
                        else
                        {
                            gState=STATE_OFF;
                        }
                        break;
                    case STATE_CONFIG:
                        gState=STATE_SETUP;
                        break;
                    case STATE_OFF:
                        if (gNumDesignatedControllers)
                        {
                            gState=STATE_CONFIG;
                        }
                        break;
                    case STATE_SHUTDOWN:
                        gState=STATE_OFF;
                        break;
                     case STATE_CONF0:
                        gState=STATE_SETUP;
                        gSetupIsRunning=false;
                        break;
                    case STATE_CONF1:
                        if (gDesignatedControllers[0].numberOfDevices != 0)
                        {
                            gState=STATE_CONF0;
                        } 
                        else 
                        {
                            gSetupIsRunning=false;
                            gState=STATE_SETUP;
                        }
                        break;
                    case STATE_LAUNCH:
                    
                        if (gCurrentGame == 0)
                        {
                            gState=STATE_OFF;
                        }
                        else
                        {
                            gCurrentGame--;
                        }
                        break;
                    case STATE_FLR_GAMES:
                        if (gDesignatedControllers[1].numberOfDevices != 0)
                        {
                            gState=STATE_CONF1;
                        } 
                        else if (gDesignatedControllers[0].numberOfDevices != 0)
                        {
                            gState=STATE_CONF0;
                        }
                        else 
                        {
                            gSetupIsRunning=false;
                            gState=STATE_SETUP;
                        }
                        break;
                    case STATE_FLR_FW:
                            gState=STATE_FLR_GAMES;
                        break;
                    default:
                        (void) 0;
                        break;
                }
                break;
            case SDL_CONTROLLER_BUTTON_B:
                if (gState == STATE_OFF) gState = STATE_SHUTDOWN;
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
                        if (gDesignatedControllers[0].numberOfDevices != 0)
                        {
                            gState=STATE_CONF0;
                        } 
                        else if (gDesignatedControllers[1].numberOfDevices != 0)
                        {
                            gState=STATE_CONF1;
                        }
                        gSetupIsRunning=true;
                        break;
                    case STATE_CONFIG:
                        freeTextures();
                        SDL_DestroyRenderer( gRenderer );
                        do
                        {
                            str = "screen_manager";
                            n = str.length(); 
                            strcpy(arg1, str.c_str()); 

                            screen_man_argv[0] = arg1;
                            screen_man_argc = 1;
                            
                            launch_request_from_screen_manager = false;
                            restart_screen_manager = false;
                            
                            screen_manager_main(screen_man_argc,screen_man_argv);
                            if (shutdown_now)
                            {
                                shutdown_computer();
                            } else
                            if (launch_request_from_screen_manager)
                            {
                                launch_emulator();
                            }
                        } while (launch_request_from_screen_manager || restart_screen_manager);
                        gQuit=true;
                        break;
                    case STATE_OFF:
                        gQuit=true;
                        break;
                    case STATE_SHUTDOWN:
                        gQuit=true;
                        shutdown_computer();
                        break;
                     case STATE_CONF0:
                     case STATE_CONF1:
                        
                        break;
                    case STATE_FLR_GAMES:
                        if (!gTextInputForGamingFolder)
                        {
                            gTextInputForGamingFolder=true;
                            gTextInput = true;
                            
                            const char *homedir;
                            string slash;
        
                            if ((homedir = getenv("HOME")) != nullptr) 
                            {
                                gText = homedir;
                                
                                slash = gText.substr(gText.length()-1,1);
                                if (slash != "/")
                                {
                                    gText += "/";
                                }
                            }
                            else
                            {
                                gText = "";
                            }
                        }
                        else
                        {
                            if (gTextInput) // input exit with RETURN
                            {
                                if (setPathToGames(gText))
                                {
                                    
                                    string setting = "search_dir_games=";
                                    setting += gText;
                                    addSettingToConfigFile(setting);
                                    
                                    gTextForGamingFolder=gText;
                                    stopSearching=false;
                                    buildGameList();
                                    checkFirmwarePSX();
                                    if (stopSearching)
                                    {
                                        gGame.clear();
                                        gGamesFound=false;
                                        gPS1_firmware=false;
                                        gPS2_firmware=false;
                                        gSegaSaturn_firmware=false;
                                    }
                                }
                            }
                            else // input exit with ESC
                            {
                                gTextForGamingFolder=gText;
                            }
                            gTextInputForGamingFolder=false;
                            gTextInput = false;
                        }
                        break;
                    case STATE_FLR_FW:
                        if (!gTextInputForFirmwareFolder)
                        {
                            gTextInputForFirmwareFolder=true;
                            gTextInput = true;
                            
                            const char *homedir;
                            string slash;
        
                            if ((homedir = getenv("HOME")) != nullptr) 
                            {
                                gText = homedir;
                                
                                slash = gText.substr(gText.length()-1,1);
                                if (slash != "/")
                                {
                                    gText += "/";
                                }
                            }
                            else
                            {
                                gText = "";
                            }
                        }
                        else
                        {
                            if (gTextInput) // input exit with RETURN
                            {
                                if (setPathToFirmware(gText))
                                {
                                    
                                    string setting = "search_dir_firmware_PSX=";
                                    setting += gText;
                                    addSettingToConfigFile(setting);
                                    
                                    gTextForFirmwareFolder=gText;
                                    checkFirmwarePSX();
                                }
                            }
                            else // input exit with ESC
                            {
                                gTextForFirmwareFolder=gText;
                            }
                            gTextInputForFirmwareFolder=false;
                            gTextInput = false;
                        }
                        break;
                    case STATE_LAUNCH:
                        launch_emulator();
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
    }
    else // controller conf
    {
        
    }
}


void statemachineConf(int cmd)
{
    DEBUG_PRINTF("   void statemachineConf(int cmd = %d), confState = %d, gActiveController = %d, gControllerConfNum = %d\n",cmd,confState,gActiveController,gControllerConfNum);
    if ((cmd==STATE_CONF_SKIP_ITEM) && (confState > STATE_CONF_BUTTON_RIGHTSHOULDER))
    {
        statemachineConfAxis(STATE_CONF_SKIP_ITEM,false);
        return;
    }
    
    if ((gActiveController == gControllerConfNum) || (cmd==STATE_CONF_SKIP_ITEM))
    {
        switch (confState)
        {
            case STATE_CONF_BUTTON_DPAD_UP:
                if (secondRun == -1)
                {
                    setControllerConfText("press dpad up","(or use ENTER to skip this button)");
                    secondRun = cmd;
                }
                else if (secondRun == cmd)
                {
                    gControllerButton[confState]=cmd;
                    confState = STATE_CONF_BUTTON_DPAD_DOWN;
                    setControllerConfText("press dpad down");
                    secondRun = -1;
                }
                break;
            case STATE_CONF_BUTTON_DPAD_DOWN:
                gControllerButton[confState]=cmd;
                confState = STATE_CONF_BUTTON_DPAD_LEFT;
                setControllerConfText("press dpad left");
                break;
            case STATE_CONF_BUTTON_DPAD_LEFT:
                gControllerButton[confState]=cmd;
                confState = STATE_CONF_BUTTON_DPAD_RIGHT;
                setControllerConfText("press dpad right");
                break;
            case STATE_CONF_BUTTON_DPAD_RIGHT:
                gControllerButton[confState]=cmd;
                confState = STATE_CONF_BUTTON_A;
                setControllerConfText("press south button (lower)");
                break;
            case STATE_CONF_BUTTON_A:
                gControllerButton[confState]=cmd;
                confState = STATE_CONF_BUTTON_B;
                setControllerConfText("press east button (right)");
                break;
            case STATE_CONF_BUTTON_B:
                gControllerButton[confState]=cmd;
                confState = STATE_CONF_BUTTON_X;
                setControllerConfText("press west button (left)");
                break;
            case STATE_CONF_BUTTON_X:
                gControllerButton[confState]=cmd;
                confState = STATE_CONF_BUTTON_Y;
                setControllerConfText("press north button (upper)");
                break;
            case STATE_CONF_BUTTON_Y:
                gControllerButton[confState]=cmd;
                confState = STATE_CONF_BUTTON_LEFTSTICK;
                setControllerConfText("press left stick button");
                break;
            case STATE_CONF_BUTTON_LEFTSTICK:
                gControllerButton[confState]=cmd;
                confState = STATE_CONF_BUTTON_RIGHTSTICK;
                setControllerConfText("press right stick button");
                break;
            case STATE_CONF_BUTTON_RIGHTSTICK:
                gControllerButton[confState]=cmd;
                confState = STATE_CONF_BUTTON_LEFTSHOULDER;
                setControllerConfText("press left front shoulder");
                break;
            case STATE_CONF_BUTTON_LEFTSHOULDER:
                gControllerButton[confState]=cmd;
                confState = STATE_CONF_BUTTON_RIGHTSHOULDER;
                setControllerConfText("press right front shoulder");
                break;
            case STATE_CONF_BUTTON_RIGHTSHOULDER:
                gControllerButton[confState]=cmd;
                confState = STATE_CONF_BUTTON_BACK;
                setControllerConfText("press select button");
                break;
            case STATE_CONF_BUTTON_BACK:
                gControllerButton[confState]=cmd;
                confState = STATE_CONF_BUTTON_START;
                setControllerConfText("press start button");
                break;
            case STATE_CONF_BUTTON_START:
                gControllerButton[confState]=cmd;
                confState = STATE_CONF_BUTTON_GUIDE;
                setControllerConfText("press guide button");
                break;
            case STATE_CONF_BUTTON_GUIDE:
                gControllerButton[confState]=cmd;
                confState = STATE_CONF_AXIS_LEFTSTICK_X;
                setControllerConfText("twirl left stick");
                xCount=0;
                yCount=0;
                xValue=-1;
                yValue=-1;
                break;
            case STATE_CONF_AXIS_LEFTTRIGGER:
                gControllerButton[STATE_CONF_BUTTON_LEFTTRIGGER]=cmd;
                confState = STATE_CONF_AXIS_RIGHTTRIGGER;
                setControllerConfText("press right rear shoulder");
                break;
            case STATE_CONF_AXIS_RIGHTTRIGGER:
                gControllerButton[STATE_CONF_BUTTON_RIGHTTRIGGER]=cmd;
                setMapping();
                break;
            default:
                (void) 0;
                break;
        }
    }
}

void statemachineConfAxis(int cmd, bool negative)
{
    if ( (gControllerConf) && (confState >= STATE_CONF_AXIS_LEFTSTICK_X) )
    {
        if ((gActiveController == gControllerConfNum)  || (cmd==STATE_CONF_SKIP_ITEM))
        {
            switch (confState)
            {
                case STATE_CONF_AXIS_LEFTSTICK_X:
                case STATE_CONF_AXIS_LEFTSTICK_Y:
                    if (checkAxis(cmd))
                    {
                        printf("***** x axis: %i, y axis: %i\n",xValue,yValue);
                        xCount=0;
                        yCount=0;
                        xValue=-1;
                        yValue=-1;

                        confState = STATE_CONF_AXIS_RIGHTSTICK_X;
                        setControllerConfText("twirl right stick");
                    }
                    break;
                case STATE_CONF_AXIS_RIGHTSTICK_X:
                case STATE_CONF_AXIS_RIGHTSTICK_Y:
                    if (cmd == STATE_CONF_SKIP_ITEM)
                    {
                        xCount=0;
                        yCount=0;
                        xValue=-1;
                        yValue=-1;
                        
                        confState = STATE_CONF_AXIS_LEFTTRIGGER;
                        setControllerConfText("press left rear shoulder");
                    }
                    else if ( (cmd != gControllerButton[STATE_CONF_AXIS_LEFTSTICK_X]) &&\
                            (cmd != gControllerButton[STATE_CONF_AXIS_LEFTSTICK_Y]))
                    {
                        
                        if (checkAxis(cmd))
                        {
                            printf("***** x axis: %i, y axis: %i\n",xValue,yValue);
                            xCount=0;
                            yCount=0;
                            xValue=-1;
                            yValue=-1;
                            
                            confState = STATE_CONF_AXIS_LEFTTRIGGER;
                            setControllerConfText("press left rear shoulder");
                        }
                    }
                    break;
                case STATE_CONF_AXIS_LEFTTRIGGER:
                    if (cmd == STATE_CONF_SKIP_ITEM)
                    {
                        xCount=0;
                        xValue=-1;
                          
                        confState = STATE_CONF_AXIS_RIGHTTRIGGER;
                        setControllerConfText("press right rear shoulder");
                    }
                    else if ( (cmd != gControllerButton[STATE_CONF_AXIS_RIGHTSTICK_X]) &&\
                            (cmd != gControllerButton[STATE_CONF_AXIS_RIGHTSTICK_Y]))
                    {
                        if (checkTrigger(cmd))
                        {
                            printf("***** trigger: %i\n",xValue);
                            xCount=0;
                            xValue=-1;
                            
                            confState = STATE_CONF_AXIS_RIGHTTRIGGER;
                            setControllerConfText("press right rear shoulder");
                        }
                    }
                    
                    break;
                case STATE_CONF_AXIS_RIGHTTRIGGER:
                    if (cmd == STATE_CONF_SKIP_ITEM)
                    {
                        xCount=0;
                        xValue=-1;
                        setMapping();
                    }
                    else if (cmd != gControllerButton[STATE_CONF_AXIS_LEFTTRIGGER]) 
                    {
                        if (checkTrigger(cmd))
                        {
                            printf("***** trigger: %i\n",xValue);
                            xCount=0;
                            xValue=-1;
                            setMapping();
                        }
                    }
                    break;
                default:
                    (void) 0;
                    break;
            }
        }
    } else if ( (gControllerConf) && (confState <= STATE_CONF_BUTTON_DPAD_RIGHT) )
    {
        if ((gActiveController == gControllerConfNum)  || (cmd==STATE_CONF_SKIP_ITEM))
        {
            gAxis[axisIterator] = cmd;
            gAxisValue[axisIterator] = negative;
            switch (confState)
            {
                case STATE_CONF_BUTTON_DPAD_UP:
                    setControllerConfText("press dpad down");
                    break;
                case STATE_CONF_BUTTON_DPAD_DOWN:
                    setControllerConfText("press dpad left");
                    break;
                case STATE_CONF_BUTTON_DPAD_LEFT:
                    setControllerConfText("press dpad right");
                    break;
                case STATE_CONF_BUTTON_DPAD_RIGHT:
                    setControllerConfText("press south button (lower)");
                    break;
                default:
                    (void) 0;
                    break;
            }
            confState++;
            axisIterator++;
        }
    }
}

bool checkAxis(int cmd)
{
    if (cmd==STATE_CONF_SKIP_ITEM) return true;
    
    bool ok = false;
    
    if ( (xCount > 20) && (yCount>20) )
    {
        gControllerButton[confState]=xValue;
        gControllerButton[confState+1]=yValue;
        ok = true;
    }
    
    if ( (xValue!=-1) && (yValue!=-1) )
    {
        if (xValue == cmd) xCount++;
        if (yValue == cmd) yCount++;
    }
    
    if ( (xValue!=-1) && (yValue==-1) )
    {
        if (xValue > cmd)
        {
            yValue = xValue;
            xValue = cmd;
        }
        else if (xValue != cmd)
        {
            yValue = cmd;
        }
    }
    
    if ( (xValue==-1) && (yValue==-1) )
    {
        xValue=cmd;
    }   
    
    printf("axis confState: %i, cmd: %i, xValue: %i, yValue: %i, xCount: %i, yCount: %i \n",confState,cmd,xValue,yValue,xCount,yCount); 
    return ok;
}

bool checkTrigger(int cmd)
{
    if (cmd==STATE_CONF_SKIP_ITEM) return true;
    bool ok = false;
    
    if ( (xCount > 20)  )
    {
        gControllerButton[confState]=xValue;
        ok = true;
    }
    
    if (xValue!=-1)
    {
        if (xValue == cmd) xCount++;
    }
    
    if (xValue==-1)
    {
        xValue=cmd;
    }   
    
    printf("axis confState: %i, cmd: %i, xValue: %i, xCount: %i\n",confState,cmd,xValue,xCount); 
    return ok;
}


void statemachineConfHat(int hat, int value)
{    
    printf("ConfHat hat: %i, value: %i\n",hat,value);
    
    gHat[hatIterator] = hat;
    gHatValue[hatIterator] = value;
    
    if (gActiveController == gControllerConfNum)
    {
        
        switch (confState)
        {
            case STATE_CONF_BUTTON_DPAD_UP:            
                if ( (secondRunHat == -1) && (secondRunValue == -1) )
                {
                    setControllerConfText("press dpad up again");
                    secondRunHat = hat;
                    secondRunValue = value;
                }
                else if ( (secondRunHat == hat) && (secondRunValue == value) )
                {
                    hatIterator++;
                    confState = STATE_CONF_BUTTON_DPAD_DOWN;
                    setControllerConfText("press dpad down");
                    secondRunHat = -1;
                    secondRunValue = -1;
                }
                break;
            case STATE_CONF_BUTTON_DPAD_DOWN:
                confState = STATE_CONF_BUTTON_DPAD_LEFT;
                setControllerConfText("press dpad left");
                hatIterator++;
                break;
            case STATE_CONF_BUTTON_DPAD_LEFT: 
                confState = STATE_CONF_BUTTON_DPAD_RIGHT;
                setControllerConfText("press dpad right");
                hatIterator++;
                break;
            case STATE_CONF_BUTTON_DPAD_RIGHT: 
                confState = STATE_CONF_BUTTON_A;
                setControllerConfText("press south button (lower)");
                hatIterator++;
                break;
            default:
                (void) 0;
                break;
        }
    }
}
