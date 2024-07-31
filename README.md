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

OSC 52 Clipboard writing
------------------------

OSC 52 is an escape sequence that allows programs in the terminal to access
the desktop's clipboard. This is now supported in ROXTerm, with some
restrictions and security measures:

* It requires VTE to be [patched](./vte-osc52.diff).
* VTE has an escape sequence length limit which means you can't paste more
  than approximately 3000 bytes at a time.
* Apps can only write the clipboard, not read it.
* You have to confirm each clipboard write by clicking a button unless you
  change a profile setting.
* OSC 52 is ignored if the originating terminal is not focused.
* OSC 52 is ignored if the terminal has a selection made with its GUI.

Each time the clipboard is written, an indicator flashes in the tab bar;
further behaviour of the indicator/button can be configured in a new section of
the profile settings. The indicator and button are also used if the clipboard
is cleared.

CSS styling
-----------

Version 3.15.1 onwards apply a CSS class name to ROXTerm's vte widget, based on
the profile name. For example, a profile name of "My Profile" applies the CSS
class name "roxterm-My-Profile". Use with care, because using different values
of properties such as "padding" in the same window could cause problems with
window geometry.

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
