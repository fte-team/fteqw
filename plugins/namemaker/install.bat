@echo off
call ..\paths.bat
mkdir %PluginsDir%
copy vm\namemaker.qvm %PluginsDir%
pause