OBJS = src/main.o src/net.o src/http.o src/jutil.o src/config.o src/player.o \
	src/ui.o src/ui_gfx.o src/ui_font.o src/ui_image.o src/ui_gpu.o \
	src/offline.o src/osk.o src/paths.o src/debug_log.o src/theme.o \
	src/ringbuf.o src/storage.o src/metrics.o src/flac_dec.o src/download.o src/updater.o \
	assets/ui/atlas.o assets/ui/font.o assets/ui/labels.o

INCDIR = src
# Release build: no DEBUG_HUD (no overlay, no debug.log). Use make DEBUG_HUD=1 for QA.
CFLAGS = -O2 -G0 -Wall -Wextra
ifdef DEBUG_HUD
    CFLAGS += -DDEBUG_HUD
endif
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =
LDFLAGS =
LIBS = \
	-lpsphttp -lpspssl \
	-lpspwlan -lpsputility \
	-lpspmp3 -lpspaudio \
	-lpspgu -lpspgum \
	-lpsppower \
	-lpng -lz -ljpeg \
	-lFLAC -logg -lm

EXTRA_TARGETS = EBOOT.PBP
APP_VER = 1.3.10
APP_VER_CODE = 160
# XMB «Версия» = DISC_VERSION in Sony format X.YZ (e.g. 1.39), not semver.
DISC_VER = $(shell printf '%d.%02d' $$(($(APP_VER_CODE)/100)) $$(($(APP_VER_CODE)%100)))
PSP_EBOOT_TITLE = PSP Music $(APP_VER)
PSP_FW_VERSION = 371
PSP_EBOOT_ICON = assets/ICON0.PNG
PSP_EBOOT_PIC1 = assets/PIC1.PNG
PSP_EBOOT_SND0 = assets/SND0.AT3
SFO_SET = python3 ../tools/sfo_set_string.py

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

# Do NOT pass -s DISC_VERSION to mksfoex: it prepends the key and breaks
# alphabetical order. XMB then fails binary search → Version shows as "-".
# Build default (sorted) SFO, then patch DISC_VERSION in place.
$(PSP_EBOOT_SFO):
	$(MKSFO) '$(PSP_EBOOT_TITLE)' $@
	$(SFO_SET) $@ DISC_VERSION $(DISC_VER)

# Rebuild LIBS: build.mak's -lpspnet/-lpspnet_apctl alone leave inet stubs split by libc.
LIBS := \
	-lpsphttp -lpspssl -lpsputility \
	-lpspmp3 -lpspaudio -lpspgu -lpspgum -lpsppower -lpspwlan \
	-lpspdebug -lpspdisplay -lpspge -lpspctrl \
	-lpng -lz -ljpeg \
	-lFLAC -logg -lm \
	-Wl,--whole-archive \
	-lpspnet -lpspnet_inet -lpspnet_apctl -lpspnet_resolver \
	-Wl,--no-whole-archive

# Embed UI atlas / font as raw ABGR8888 blobs (power-of-two friendly).
assets/ui/%.o: assets/ui/%.rgba
	psp-objcopy -I binary -O elf32-littlemips -B mips $< $@

# Permanent XMB icon: Game → "Music Updater" (own icon/pic/snd0). Never delete at runtime.
companion: EBOOT.PBP
	rm -rf dist/PSPMUSICUPD
	mkdir -p dist/PSPMUSICUPD
	psp-strip $(TARGET).elf -o _upd_strip.elf
	$(MKSFO) 'Music Updater $(APP_VER)' dist/PSPMUSICUPD/PARAM.SFO
	$(SFO_SET) dist/PSPMUSICUPD/PARAM.SFO DISC_VERSION $(DISC_VER)
	pack-pbp dist/PSPMUSICUPD/EBOOT.PBP dist/PSPMUSICUPD/PARAM.SFO \
		assets/updater/ICON0.PNG NULL NULL assets/updater/PIC1.PNG assets/updater/SND0.AT3 \
		_upd_strip.elf NULL
	rm -f _upd_strip.elf dist/PSPMUSICUPD/PARAM.SFO
	@echo "Built dist/PSPMUSICUPD/EBOOT.PBP (Music Updater) DISC_VERSION=$(DISC_VER)"

.PHONY: companion
