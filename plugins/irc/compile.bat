@echo off
call ..\paths.bat

del vm\*.asm
rmdir vm
mkdir vm
cd vm
lcc -DQ3_VM -S -Wf-target=bytecode -Wf-g ../ircclient.c
lcc -DQ3_VM -S -Wf-target=bytecode -Wf-g ../../plugin.c
lcc -DQ3_VM -S -Wf-target=bytecode -Wf-g ../../qvm_api.c
q3asm -f ../ircclient

cd ..

pause