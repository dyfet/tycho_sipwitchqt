What is SipWitchQt
==================

[![pipeline status](https://gitlab.com/tychosoft/sipwitchqt/badges/master/pipeline.svg)](https://gitlab.com/tychosoft/sipwitchqt/commits/master)

This is a new enterprise sip server which extends sipwitch branding and re-thinks my original sipwitch architecture.  This new server is written using [Qt](https://www.qt.io) and C++11.  This package also introduces a new general purpose daemon service architecture for Qt rather than using Qt Service, and this is embedded thru src/Common.pri.  This package includes a desktop sip user/messaging agent based on work in GFC antisipate, and will include a mobile client as well.

One key difference between SipWitchQt and GNU SIPWitch is that all of the management to configure and control this new server is now contained in a simple Sql backend database.  Qt's Sql plugin support is used for generic access to backend databases such as MySQL, SQLite, etc.  Frictionless call support is to be achieved by isolating backend db queries to a separate thread and by smart caching of db records for active extensions.  By using a generic Sql backend, it also becomes very easy to write web based administrative and control front ends for SipWitchQt.

Development of this project is managed from https://gitlab.com/tychosoft/sipwitchqt/ and the github repo is updated only occasionally.

## Dependencies

SipWitchQt should build with any Qt version later than 5.5.  Official binary installers will be built from latest LTS (currently 5.9).  SipWitchQt clients and servers also require libeXosip2 4.0 or later to build.  For really old Unix/Linux distros that may only have very old versions of exosip2 in their repos, we locally build libeXosip2 from the vendor/ directory.  In the case of fedora and redhat, which both have very old libeXosip2 releases but each use different openssl api's, we have a copr repo, at dyfet/stable and dyfet/develop.  For server zeroconfig support you should install libavahi-client-dev.  For systems using systemd, you should install libsystemd-dev.

For building on MacOS you can use homebrew.  The libraries are linked and used thru the /usr/local/opt/xxx paths to avoid conflicts when building with the Qt company online installer and provided libraries.  This solves the problem where things like the brew version of libjpeg breaking linkage for the Qt distributed libs.  To install libeXosip2 with homebrew, simply do:

``brew install libexosip``

This should pull in all other required dependencies (openssl, libosip, etc) on MacOS.  You can also build using the homebrew version of Qt.

For building on Microsoft Windows, the repo contains a vendored and lightly modified eXosip2.  To get openssl to build with exosip2 you must use VCPKG, set the CMAKE_TOOLCHAIN_FILE as directed, and install an appropriate triple for openssl, such as vcpkg install openssl:w64-windows.  In the past we supplied a vendored openssl build kit, and we no longer are doing this.  Release builds for windows also automatically create an installer for Microsoft Windows using inno setup.

## Deploy

This project is not yet far enough along for deployment or use, but I do have some specific plans for this.  For GNU/Linux systems, I will support both init scripts and systemd units, which will be added to a new top level etc directory in the source tree, along with things like a logrotate.d file.  

In addition to os distribution packages and port files, I am looking at providing docker instances.  The idea is to be able to deploy and use SipWitchQt anywhere; on premise or in the cloud.  To better support this, the SipWitchQt docker image should also be composable into a stack that includes a separated database server and web interface.  The web interface will be worked on as a separate project to be called switchroom.  The idea is a complete and easily managed deploy anywhere enterprise VoIP phone system.  For windows users and possibly mobile, hockeyapp may be used to distribute testing builds.

## Documentation

Generation of source documentation can be done using doxygen with the provided Doxyfile using the docs build target.  This is setup to generate pdf and latex documentation as well as html pages, an xcode docset, a qt assistant stream, and a windows help file.  From qtcreator you can add a "docs" make target to your project (debug) build steps, and enable it to create or update documentation.

When enabling the docs make target from QtCreator itself you will be given warnings for undocumented classes as issues.  This makes it easy to find and complete documentation for the header files.  You can also use "make docs" from command line builds.

Source documentation is meant only to document the class and design architecture of the
sipwitchqt code base.  User and administration documentation will be written separately as something like "CONFIG.md" and/or "SETUP.md" once development is further along.  Design notes may be added as "DESIGN.md" or in CONTRIBUTING.md in the future.

## Support

At the moment I do not have infrastructure to offer support.  I have published this, and perhaps as of this writing, other telephony packages, to develop best practices for producing a new generation of telephony services.  I do maintain an email address for public contact for all similarly published Tycho Softworks projects as [tychosoft@gmail.com](mailto://tychosoft@gmail.com).  Merge requests may be accepted when I happen to have a chance and connectivity to do so.  I also will be using the gitlab [sipwitchqt](https://gitlab.com/tychosoft/sipwitchqt) issue tracker for bug reporting and project management.  Maybe in the future we will be able to put together a and commercially deliver a complete VoIP solution. 

## Testing

The cmake debug config builds a test server that uses the source "testdata/" directory to store databases and running files.  This is used to provide a stable and repeatable test environment.  An alternate userdata/ directory can be created as private per-developer test environment.  Currently the server binds for ipv4, so you have to use 127.0.0.1:4060 for localhost if localhost is set to ::1.  I am also adding SIPp test cases for the sipwitch server in the testdata directory.

## Release installation

To install the server properly in usual directories on Linux and use provided systemd sipwitchd.service fike need to run
cmake  -DCMAKE_INSTALL_SYSCONFDIR=/etc -DCMAKE_INSTALL_LOCALSTATEDIR=/var/lib -DCMAKE_INSTALL_PREFIX=/usr .
Or if you change directories you will need to change the service file in /usr/lib/systemd/system/sipwitchd.service
