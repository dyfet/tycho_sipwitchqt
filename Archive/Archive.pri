# source publication

QMAKE_EXTRA_TARGETS += publish
publish.commands += cd $${PWD}/.. &&

win32 {
    publish.commands += del $${OUT_PWD}/$${ARCHIVE}-$${VERSION}.zip &&
    publish.commands += git archive --format zip -o $${OUT_PWD}/$${ARCHIVE}-$${VERSION}.zip --prefix=$${ARCHIVE}-$${VERSION}/ HEAD
}

unix {
    publish.commands += rm -f $${OUT_PWD}/$${ARCHIVE}-${VERSION}.tar.gz &&
    publish.commands += git archive --format tar --prefix=$${ARCHIVE}-$${VERSION}/ HEAD |
    publish.commands += gzip >$${OUT_PWD}/$${ARCHIVE}-$${VERSION}.tar.gz
}

# documentation processing

exists(../Doxyfile) {
    QMAKE_EXTRA_TARGETS += docs

    unix:publish.depends += docs
    unix:publish.commands += && cp $${OUT_PWD}/doc/latex/refman.pdf $${OUT_PWD}/$${ARCHIVE}-$${VERSION}.pdf

    DOXYPATH = $${PWD}/..
    doxyfile.input = $${PWD}/../Doxyfile
    doxyfile.output = $${OUT_PWD}/Doxyfile.out
    QMAKE_SUBSTITUTES += doxyfile

    macx:docs.commands += PATH=/usr/local/bin:/usr/bin:/bin:/Library/Tex/texbin:$PATH && export PATH &&
    docs.commands += cd $${OUT_PWD} && doxygen Doxyfile.out
    macx:docs.commands += && cd doc/html && make docset
    unix:docs.commands += && cd ../latex && make
}

# binary packages, for release builds only...

macx:CONFIG(release, release|debug):CONFIG(app_bundle) {
    QMAKE_EXTRA_TARGETS += archive publish_and_archive
    archive.depends = all
    archive.commands += $${PWD}/Archive.sh $${TARGET}
    publish_and_archive.depends = publish archive
}

win32:CONFIG(release, release|debug) {
    QMAKE_EXTRA_TARGETS += archive publish_and_archive
    archive.depends = all
    archive.commands += $${PWD}/Archive.cmd $${TARGET}
    publish_and_archive.depends = publish archive
}

OTHER_FILES += \
    $${PWD}/Archive.sh \
    $${PWD}/Archive.cmd \
    $${PWD}/Archive.bld \
