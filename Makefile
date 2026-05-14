APP_NAME = system-monitor
APP_BINARY = app/build/system-monitor-app
VERSION = 3.0.0
DIST_DIR = dist
DEB_ROOT = $(DIST_DIR)/$(APP_NAME)_$(VERSION)
DEB_FILE = $(DIST_DIR)/$(APP_NAME)_$(VERSION)_amd64.deb

all:
	$(MAKE) -C backend
	$(MAKE) -C app

run:
	$(MAKE) -C app run

test:
	$(MAKE) -f Makefile.test run

clean:
	$(MAKE) -C backend clean
	$(MAKE) -C app clean
	$(MAKE) -f Makefile.test clean
	rm -rf $(DIST_DIR)

package-deb: all
	rm -rf $(DEB_ROOT)
	mkdir -p $(DEB_ROOT)/DEBIAN
	mkdir -p $(DEB_ROOT)/usr/local/bin
	mkdir -p $(DEB_ROOT)/usr/share/applications
	mkdir -p $(DEB_ROOT)/usr/share/icons/hicolor/scalable/apps
	mkdir -p $(DEB_ROOT)/usr/share/doc/$(APP_NAME)
	cp $(APP_BINARY) $(DEB_ROOT)/usr/local/bin/system-monitor
	cp app/assets/system-monitor.svg $(DEB_ROOT)/usr/share/icons/hicolor/scalable/apps/system-monitor.svg
	printf 'Package: system-monitor\nVersion: $(VERSION)\nSection: utils\nPriority: optional\nArchitecture: amd64\nDepends: libgtk-3-0, libglib2.0-0, libgdk-pixbuf-2.0-0, libcairo2, libpango-1.0-0, coreutils, procps\nMaintainer: System Monitor Maintainers <maintainer@example.com>\nDescription: Native GTK system monitor\n A native Ubuntu desktop system monitor written in C.\n' > $(DEB_ROOT)/DEBIAN/control
	printf '[Desktop Entry]\nType=Application\nName=System Monitor\nComment=Native system monitor dashboard\nExec=/usr/local/bin/system-monitor\nIcon=system-monitor\nTerminal=false\nCategories=System;Monitor;GTK;\nStartupNotify=true\n' > $(DEB_ROOT)/usr/share/applications/system-monitor.desktop
	printf 'system-monitor ($(VERSION)) stable; urgency=medium\n\n  * Native GTK system monitor release.\n  * Adds packaged desktop icon and runtime metadata.\n\n -- System Monitor Maintainers <maintainer@example.com>  Fri, 15 May 2026 00:00:00 +0700\n' > $(DEB_ROOT)/usr/share/doc/$(APP_NAME)/changelog.Debian
	gzip -9 -n $(DEB_ROOT)/usr/share/doc/$(APP_NAME)/changelog.Debian
	printf 'Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/\nUpstream-Name: system-monitor\nSource: local project\n\nFiles: *\nCopyright: 2026 System Monitor Maintainers\nLicense: MIT\n Permission is hereby granted, free of charge, to any person obtaining a copy\n of this software and associated documentation files (the "Software"), to deal\n in the Software without restriction, including without limitation the rights\n to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n copies of the Software, and to permit persons to whom the Software is\n furnished to do so.\n' > $(DEB_ROOT)/usr/share/doc/$(APP_NAME)/copyright
	dpkg-deb --build $(DEB_ROOT) $(DEB_FILE)
	@echo "Created $(DEB_FILE)"

.PHONY: all run test clean package-deb
