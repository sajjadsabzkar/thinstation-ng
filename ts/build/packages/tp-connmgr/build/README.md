# tp-connmgr

The connection chooser: a Qt5 Widgets application that reads connection
definitions from the `tp-registry` and launches them through ThinStation's
own `pkg` dispatcher.

It is modelled on HP ThinPro's Connection Manager, including the split
between a normal window and a locked-down full-screen presentation, but it
carries none of ThinPro's KDE Frameworks dependencies. It needs only
QtCore, QtGui and QtWidgets.

## How a connection runs

```
tp-connmgr  (or tp-autostart, or tp-launch by hand)
     |
     v
tp-launch <uuid>
     |  resolves the connection from the registry
     v
pkg <mode> <package> [server] [options] [workspace] [user] ...
     |
     v
/etc/init.d/<package>   =  thinstation.packages
     |
     v
xfreerdp / the Citrix client / the Horizon client / firefox
```

Reusing `pkg` rather than driving the clients directly is deliberate: every
ThinStation package already carries its own credential dialogs, reconnect
logic and status handling, and this way all of it keeps working.

## Building the binary

ThinStation packages do not compile anything - they repackage prebuilt
Fedora RPMs. The precedent for shipping our own program is `gtkdialog`,
whose binary is committed straight into the package. `tp-connmgr` follows
that: the source lives in `src/`, and the built binary is committed to
`bin/tp-connmgr`.

Rebuild it after changing anything under `src/`:

```sh
docker build -t tp-qtbuild:f42 - <<'DOCKERFILE'
FROM fedora:42
RUN dnf install -y qt5-qtbase-devel gcc-c++ make && dnf clean all
DOCKERFILE

docker run --rm -v "$PWD/src":/src:ro -v "$PWD/bin":/out tp-qtbuild:f42 sh -c '
    cp -r /src /build && cd /build &&
    qmake-qt5 tp-connmgr.pro && make -j"$(nproc)" &&
    cp tp-connmgr /out/'
```

Build it against the same Fedora release the image is built from, so it
links against the same Qt and glibc.

## Running it outside an image

Both the app and `tpreg` honour `TP_DEFAULTS` and `TP_REGISTRY`, so you can
point them at a scratch registry:

```sh
export TP_DEFAULTS=$PWD/../tp-registry/etc/tp/registry.defaults
export TP_REGISTRY=/tmp/registry.conf
./bin/tp-connmgr
```

`tp-launch` will not get far outside an image - it needs `/etc/thinstation.global`
and the `pkg` dispatcher - but the chooser, the editor and the registry all work.
