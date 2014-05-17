cl -O1 -MD -DWIN32 -c LuaBridge.c
link /DLL /out:LuaBridge.dll /SUBSYSTEM:WINDOWS LuaBridge.obj /def:LuaBridge.def /base:0x00d40000 