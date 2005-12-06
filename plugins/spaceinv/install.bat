@echo off
call ..\paths.bat
mkdir %PluginsDir%
copy vm\spaceinv.qvm %PluginsDir%
pause