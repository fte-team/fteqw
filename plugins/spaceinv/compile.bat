@echo off
call ..\paths.bat

del vm\*.asm
rmdir vm
mkdir vm
cd vm
lcc -DQ3_VM -S -Wf-target=bytecode -Wf-g ../spaceinv.c
if errorlevel 1 goto end
lcc -DQ3_VM -S -Wf-target=bytecode -Wf-g ../../memory.c -DMEMSIZE=1048576
if errorlevel 1 goto end
lcc -DQ3_VM -S -Wf-target=bytecode -Wf-g ../../plugin.c
if errorlevel 1 goto end
lcc -DQ3_VM -S -Wf-target=bytecode -Wf-g ../../qvm_api.c
if errorlevel 1 goto end
q3asm -f ../spaceinv

:end

cd ..

pause