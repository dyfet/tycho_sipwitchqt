What is SipWitchQt
==================

This is a newly written from scratch enterprise sip server which extends sipwitch branding and re-thinks my original sipwitch architecture.  This new server is written using [Qt](https://www.qt.io) and C++11.  This package also introduces a new general purpose daemon service architecture for Qt rather than using Qt Service, and this is embedded thru src/Common.pri.

One key difference between SipWitchQt and GNU SIPWitch is that all of the management to configure and control this new server is now contained in a simple Sql backend database.  Qt's Sql plugin support is used for generic access to backend databases such as MySQL, SQLite, etc.  Frictionless call support is to be achieved by isolating backend db queries to a separate thread and by smart caching of db records for active extensions.  By using a generic Sql backend, it also becomes very easy to write web based administrative and control front ends for SipWitchQt.

Archive
=======

Archive is used by someone acting as a project maintainer to do a number of maintainer functions by extra make targets that most people using this project may not need to use.  This includes generating source, binary, and documentation packages for official distribution.  For this reason, and because it is boiler plate qmake support that can be re-used in multiple projects, it is provided through a git submodule.  Perhaps the one valuable feature for more general use is support for doxygen generation with the ``docs`` target.

Bootstrap
=========

Bootstrap is only required to build sipwitch on Microsoft Windows, by providing runtime libraries and headers that are not included with Microsoft Windows or Visual Studio development environments.  To do this, I use cmake and take advantage of cmake external projects support.  Cmake is then invoked directly from qmake as part of the project configuration.  Bootstrap offers openssl libraries as well as libexosip2 for sipwitchqt.  The libeXosip2 stack will not currently build on any version of MSVC later than 2013, and is recommended for building SipWitchQt with Qt on Microsoft Windows.

Bootstrap is implemented as a submodule project.  It only must be included when building for Microsoft Windows.  There is bootstrap support for macOS, and other unix platforms, but it is currently disabled in the project file.  By making bootstrap a submodule, and requiring git lfs in the submodule only, it is both easy to maintain library dependencies, and to avoid having to carry large 3rd-party library source tarballs inside our git repos.  This also isolates the requirement for git-lfs to Microsoft Windows development environments only.

For really old Unix/Linux distros that may only have very old versions of exosip2 in their repos, you can still enable bootstrap by modifying the project file, though it is recommended instead to package newer libraries for these.  For Fedora (and eventually Redhat), which also has very old versions of libeXosip2, I currently maintain copr repos.  For current fedora distros you can use: 

``dnf copr enable dyfet/develop``.

Deploy
======

This project is not yet far enough along for deployment or use, but I do have some specific plans for this.  For GNU/Linux systems, I will support both init scripts and systemd units, which will be added to a new top level etc directory in the source tree, along with things like a logrotate.d file.  I guess on something like macOS it could be built from homebrew directly and added to launchd.  On Microsoft Windows it will eventually have code to support registering itself as a system service.

In addition to os distribution packages and port files, I am looking at providing docker instances.  The idea is to be able to deploy and use SipWitchQt anywhere; on premise or in the cloud.  To better support this, the SipWitchQt docker image should also be composable into a stack that includes a separated database server and web interface.  The web interface will be worked on as a separate project to be called switchroom.  The idea is a complete and easily managed deploy anywhere enterprise VoIP phone system.

On Microsoft Windows, perhaps SipWitchQt could be bundled with a SipBayonneQt server and other things as part of a complete VoIP service stack that can be delivered in an inno setup installer that shares Qt runtime libraries.  On macOS one option would be to build SipWitchQt using the Qt online installer, and then create a bundled executable including Qt runtime that could be installed as a macos .pkg.

Documentation
=============

Generation of source documentation can be done using doxygen with the provided Doxyfile using the Archive git submodule.  This is setup to generate pdf and latex documentation as well as html pages, an xcode docset, a qt assistant stream, and a windows help file.  From qtcreator you can add a "docs" make target to your project (debug) build steps, and enable it to create or update documentation.

When enabling the docs make target from QtCreator itself you will be given warnings for undocumented classes as issues.  This makes it easy to find and complete documentation for the header files.  You can also use "make docs" from command line builds.

Source documentation is meant only to document the class and design architecture of the
sipwitchqt code base.  User and administration documentation will be written separately as something like "CONFIG.md" and/or "SETUP.md" once development is further along.  Design notes may be added as "DESIGN.md" or in CONTRIBUTING.md in the future.

If you do not have the Archive git submodule, or you simply wish to generate the documentation in the local source subdirectory, you can use something like:

```
sed -e "s/[$][$]DOXYPATH/./g" <Doxyfile >Doxyfile.out
doxygen Doxyfile.out
```

Homebrew
========

For macOS support I new use homebrew.  The libraries
are linked and used thru the /usr/local/opt/xxx paths to avoid conflicts when building with the Qt company online installer and provided libraries.  This solves the problem where things like the brew version of libjpeg breaking linkage for the Qt distributed libs.  To use homebrew, simply do:

``brew install libexosip``

This should pull in all other required dependencies (openssl, libosip, etc).

Support
=======

At the moment I do not have infrastructure to offer support.  I have published this, and perhaps as of this writing, other telephony packages, to develop best practices for producing a new generation of telephony services.  I do maintain an email address for public contact for all similarly published Tycho Softworks projects as [tychosoft@gmail.com](mailto://tychosoft@gmail.com).  Merge requests may be accepted when I happen to have a chance and connectivity to do so.  I also will be using the gitlab [sipwitchqt](https://gitlab.com/tychosoft/sipwitchqt) issue tracker for bug reporting and project management.  Maybe in the future we will be able to put together a and commercially deliver a complete VoIP solution. 
