# tp-controlpanel

The settings shell, modelled on HP ThinPro's Control Panel.

ThinPro keeps one `.desktop` file per panel in
`/etc/hptc-control-panel/applications`, shows the ones whose `Categories`
contain `config-panel`, groups them by `X-HPTC-Category`, and decides what a
non-admin may see from a second directory, `users/<name>/applications`. Each
panel is then its own binary -- 36 shared-object plugins under
`/usr/lib/manticore/config-panels`.

The first half of that we copy exactly, under `/etc/tp/control-panel` with
the keys renamed to `X-TP-Category` and `X-TP-ExecAsRoot`. The second half we
do not: 36 binaries is a lot of image for a client with 1 GB of RAM, and
every one of those panels is a form over a handful of registry keys. So here
a panel is data.

    tp-controlpanel          the shell
    tp-panel <id>            renders any panel from root/ControlPanel/<id>
    /etc/tp/panels/<id>.apply   turns the saved keys into amixer, xrandr,
                                setxkbmap, ip, ... calls

Adding a panel means a `.desktop` file, a block in `registry.defaults` and an
`.apply` script. No C++.

## Kiosk mode

There is no kiosk key, in ThinPro or here. On ThinPro, `switch-config` writes
`PRODUCT_CONFIG=zero` into `/etc/systeminfo` *and* turns on `autostart` and
`autoReconnect` on the default connection. `customization.apply` is our
version of that script: picking `zero` in the Customization Center is what
sets `root/users/user/kioskMode`, forces the default connection to full
screen with autostart and autoReconnect, and hides the taskbar.
