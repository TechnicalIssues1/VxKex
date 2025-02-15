#pragma once

//
// Set to TRUE if you want VxlView to immediately prompt for a file if started
// with no command-line arguments.
//
#define PROMPT_FOR_FILE_ON_STARTUP TRUE

#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "uxtheme.lib")

#define NOGDICAPMASKS
#define NOVIRTUALKEYCODES
#define NOKEYSTATES
#define NOSYSCOMMANDS
#define NORASTEROPS
#define NOATOM
#define NOCOLOR
#define NODRAWTEXT
#define NOMB
#define NOMEMMGR
#define NOOPENFILE
#define NOSCROLL
#define NOTEXTMETRIC
#define NOWH
#define NOCOMM
#define NOKANJI
#define NOHELP
#define NOPROFILER
#define NODEFERWINDOWPOS
#define NOMCX
#define NOCRYPT
#define NOMETAFILE
#define NOSERVICE
#define NOSOUND

#define KEX_COMPONENT L"VxlView"
#define KEX_TARGET_TYPE_EXE
