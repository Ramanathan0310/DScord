# Top-level build. Handheld targets need devkitPro (DEVKITARM, libnds, dswifi,
# libfat, libctru); the relay needs Node 22+.
.PHONY: all nds 3ds relay check clean

all: nds 3ds

nds:
	$(MAKE) -C src/nds
	@mkdir -p dist && cp src/nds/dscord.nds dist/

3ds:
	$(MAKE) -C src/3ds
	@mkdir -p dist && cp src/3ds/dscord.3dsx dist/

relay:
	node relay/relay.mjs

check:
	node --check relay/relay.mjs

clean:
	$(MAKE) -C src/nds clean
	$(MAKE) -C src/3ds clean
	rm -rf dist
