![](xpet_logo_10x.png)  

#### xpet is a small desktop pet for x11 written in c

---

## build

dependencies:

* Xlib
* Xext
* Xpm

build with:

```
make
```

#### you SHOULD edit `config.h` before building to change default behaviour or key bindings. this includes changing the pet default directory

Run with:

```
./xpet
```

---

## image structure

Animation frames are stored as XPM files under `PET_ASSET_DIR`, grouped by state name.

example:

```
neko/
  idle/
    0.xpm
    ...
    5.xpm
  sleeping/
    0.xpm
    ...
    5.xpm
  walk_north/
    0.xpm
    1.xpm
...
```

each directory name matches an animation state defined in `xpet.h`.  
frames are loaded in numeric order (`0.xpm`, `1.xpm`, ...) until no more are found  
