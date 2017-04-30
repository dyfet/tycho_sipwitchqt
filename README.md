What is SipWitchEs
==================

This is a newly written from scratch enterprise sip server which extends sipwitch branding and re-thinks my original sipwitch architecture.  This new server is written using [Qt](https://www.qt.io) and C++11.  This package also introduces a new general purpose daemon service architecture for Qt rather than using Qt Service, and this is embedded thru src/Common.pri.  Flattening the name to lower case reduces to the amusing plural, sipwitches, as if uno bruja de sip had not been trouble enough.

One key difference between SipWitchEs and GNU SIPWitch is that all of the management to configure and control this new server is now contained in a simple Sql backend database.  Qt's Sql plugin support is used for generic access to backend databases such as MySQL, SQLite, etc.  Frictionless call support is to be achieved by isolating backend db queries to a separate thread and by smart caching of db records for active extensions.  By using a generic Sql backend, it also becomes very easy to write web based administrative and control front ends for SipWitchEs.

Bootstrap
=========

Bootstrap is used to configure dependencies and libraries that may normally not be present in your development system.  To do this, I take advantage of cmake external projects to build required libraries and invoke it directly from qmake to establish required dependencies.  In particular, for windows, bootstrap offers openssl support as well as libexosip2 for sipwitchqt and so is enabled by default.  This does mean cmake is required to build on windows and macOS.

I collect and use source tarballs found at http://pub.cherokeesofidaho.org/bootstrap.  This is done for several reasons.  I did not want to carry and manage large binary tarballs in the git repos.  I also don't want to use any other external locations such as git lfs does, or original archive sites, because often I have found myself, sometimes for months at a time, in places where I had no internet connectivity at all.  I could always do a 127.0.0.1 remap of my own pub site for such circumstances.

On more sane Unix (BSD, Linux) systems you normally do not need bootstrap.  For older Unix/Linux distros that may only very old versions of exosip2 in their repos, you can still activate bootstrap, though it is recommended instead to package newer libraries for these.  For Fedora (and later Redhat), which also has very old versions of libeXosip2, I currently maintain copr repos.  For current fedora distros you can use: 

``dnf copr enable dyfet/develop``.

For macOS I may not require bootstrap either when building for macports or homebrew.  However, I recommend bootstrap when using QtCreator downloaded from the Qt Company.  This is because even though macports (and brew) may provide dependencies I need, the pre-built Qt for macOS can break if /opt/local/lib (or /usr/local/lib) is in the link chain, particularly because both brew and macports use an incompatible version of libjpeg.  Also the version of eXosip2 brew provides is not built against c-ares, limiting performance and functionality.  I also wanted to simplify setting up a build environment for macOS without requiring external porting kits such as macports, brew, or additional packages.

Bootstrap may also be useful when setting up single use / single execution ci container environments for repeatable builds, such as travis, where dependent packages may not be installable or otherwise available.  For ci environments like Jenkins (and some uses of gitlab-ci), you may be better off taking advantage of persistent slave build environments that are pre-configured with required dependencies pre-installed instead.

Deploy
======

This project is not yet far enough along for deployment or use, but I do have some specific plans for this.  For GNU/Linux systems, I will support both init scripts and systemd units, which will be added to a new top level etc directory in the source tree, along with things like a logrotate.d file.  I guess on something like macos it could be built from homebrew or macports and added to launchd.  On Microsoft Windows it would have to have code to support registering itself as a system service.

In addition to os distribution packages and port files, I am looking at providing docker instances.  The idea is to be able to deploy and use SipWitchEs anywhere; on premise or in the cloud.  To better support this, the SipWitchEs docker image should also be composable into a stack that includes a separated database server and web interface.  The web interface will be worked on as a separate project to be called switchroom.  The idea is a complete and easily managed deploy anywhere enterprise VoIP phone system.

On Microsoft Windows, perhaps SipWitchEs could be bundled with a BayonneSip server and other things as part of a complete VoIP service stack that can be delivered in an inno setup installer that shares Qt runtime libraries.  On macos one option would be to build SipWitchEs using the Qt online installer, and then create a bundled executable including Qt runtime that could be installed as a macos .pkg.


Support
=======

At the moment I do not have infrastructure to offer support.  I have published this, and perhaps as of this writing, other telephony packages, to develop best practices for producing a new generation of telephony services.  I do maintain an email address for public contact for all similarly published Tycho Softworks projects as [tychosoft@gmail.com](mailto://tychosoft@gmail.com).  Merge requests may be accepted when I happen to have a chance and connectivity to do so.  I also will be using the gitlab [sipwitches](https://gitlab.com/tychosoft/sipwitches) issue tracker for bug reporting and project management.  Maybe in the future we will be able to put together a and commercially deliver a complete VoIP solution. 
