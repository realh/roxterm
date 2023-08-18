ROXTerm
=======

After a long break (literally) ROXTerm is making a comeback on
[github](https://github.com/realh/roxterm). Its original home was on
[Sourceforge](https://roxterm.sourceforge.net).


ROXTerm is a terminal emulator intended to provide similar features to
gnome-terminal, based on the same VTE library. It was originally designed to
have a smaller footprint and quicker start-up time by not using the Gnome
libraries and by using a separate applet to provide the configuration GUI, but
thanks to all the features it's acquired over the years ROXTerm can probably
now be accused of bloat. However, it is more configurable than gnome-terminal
and aimed more at "power" users who make heavy use of terminals.

It still supports the ROX desktop application layout it was named after, but
can also be installed in a more conventional manner for use in other desktop
environments.

What's new
----------

The [Debian changelog](./debian/changelog) provides a good summary of what's
changed between versions.

Kinetic scrolling
-----------------

Recent versions of ROXTerm, in conjunction with recent versions of vte, support
kinetic scrolling with libinput, which considerably improves the UX on laptops
for users of Wayland, and for xorg users who can't or prefer not to use the
deprecated synaptics driver. ROXTerm can detect whether the feature is
supported in vte at run-time, so no re-compilation is necessary if you upgrade
from an older vte.

Related sites
-------------

* [User guide](https://realh.github.io/roxterm),
* [Ubuntu PPA](https://launchpad.net/~h-realh/+archive/ubuntu/roxterm)
* [roxterm-git in AUR](https://aur.archlinux.org/packages/roxterm-git/)

[Tempus Themes](https://gitlab.com/protesilaos/tempus-themes) should have
support for roxterm by the time you read this. If not, you can use
[convert-tempus](https://gitlab.com/realh69/convert-tempus).
