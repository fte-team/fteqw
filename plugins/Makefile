
all: ezscript hud irc

clean: ezscript-clean hud-clean irc-clean

.PHONY: all ezscript hud irc

help:
	@-echo make a subdirectory

ezscript:
	$(MAKE) -C ezscript

ezscript-clean:
	$(MAKE) clean -C ezscript

hud:
	$(MAKE) -C hud

hud-clean:
	$(MAKE) clean -C hud

irc:
	$(MAKE) -C irc

irc-clean:
	$(MAKE) clean -C irc
	
