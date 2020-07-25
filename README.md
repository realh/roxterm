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

Kinetic scrolling
-----------------

Recent versions of ROXTerm support kinetic scrolling with libinput, which
considerably improves the UX on laptops for users of Wayland, and for xorg
users who can't or prefer not to use the deprecated synaptics driver. However,
it requires a [patched fork of vte](https://gitlab.gnome.org/realh69/vte). This
version is also available in
[AUR](https://aur.archlinux.org/packages/vte3-kinetic/).
ROXTerm can now detect the patch at run-time without having to be recompiled.
If you like this feature please visit
[this vte issue](https://gitlab.gnome.org/GNOME/vte/-/issues/234) (you may get
a 500 error at the moment dues to a gitlab problem) and give it an upvote.

Related sites
-------------

* [User guide](https://realh.github.io/roxterm),
* [Ubuntu PPA](https://launchpad.net/~h-realh/+archive/ubuntu/roxterm)
* [roxterm-git in AUR](https://aur.archlinux.org/packages/roxterm-git/)

[Tempus Themes](https://gitlab.com/protesilaos/tempus-themes) should have
support for roxterm by the time you read this. If not, you can use
[convert-tempus](https://gitlab.com/realh69/convert-tempus).
