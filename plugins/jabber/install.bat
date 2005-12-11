@echo off
call ..\paths.bat
mkdir %PluginsDir%
copy vm\jabbercl.qvm %PluginsDir%
pause