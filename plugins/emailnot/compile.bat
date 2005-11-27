@echo off
call ..\paths.bat

del vm\*.asm
rmdir vm
mkdir vm
cd vm
lcc -DQ3_VM -S -Wf-target=bytecode -Wf-g ../imapnoti.c
if errorlevel 1 goto error
lcc -DQ3_VM -S -Wf-target=bytecode -Wf-g ../md5.c
if errorlevel 1 goto error
lcc -DQ3_VM -S -Wf-target=bytecode -Wf-g ../pop3noti.c
if errorlevel 1 goto error
lcc -DQ3_VM -S -Wf-target=bytecode -Wf-g ../../memory.c
if errorlevel 1 goto error
lcc -DQ3_VM -S -Wf-target=bytecode -Wf-g ../../plugin.c
if errorlevel 1 goto error
lcc -DQ3_VM -S -Wf-target=bytecode -Wf-g ../../qvm_api.c
if errorlevel 1 goto error
q3asm -f ../emailnot

:error
cd ..

pause
goto endbat


:endbat