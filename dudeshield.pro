QT       += core gui network serialport multimedia

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

#Extract Git version from Tag
  GIT_VERSION=$$system(git --git-dir $$PWD/.git --work-tree $$PWD describe --abbrev=0 --tags)
  GIT_COMMIT=$$system(git rev-parse --short HEAD)

isEmpty(GIT_VERSION) {
# Application Version
  VERSION_MAJOR = 1
  VERSION_MINOR = 0
  VERSION_BUILD = 1
} else {
  VERSIONS = $$split(GIT_VERSION, ".")
  VERSION_MAJOR = $$member(VERSIONS, 0)
  VERSION_MINOR = $$member(VERSIONS, 1)
  VERSION_BUILD = $${GIT_COMMIT}
  message(Find Git Version : $$VERSIONS)
}

DEFINES += APP_NAME=\\\"$${TARGET}\\\"
DEFINES += APP_MAJOR=$$VERSION_MAJOR
DEFINES += APP_MINOR=$$VERSION_MINOR
DEFINES += APP_BUILD=\\\"$${VERSION_BUILD}\\\"

QMAKE_EXTRA_TARGETS += versionTarget

#DEFINES += QT_DEPRECATED_WARNINGS
#DEFINES += USE_FLITE
DEFINES += USE_SWTX

CONFIG += build_all c++14
CONFIG -= debug_and_release debug_and_release_target

TARGET = dudeshield
TEMPLATE = app

TRANSLATIONS= \
    translations/$${TARGET}_fr_FR.ts

# =======================================================================
# Copy environment variables into qmake variables
DEPLOY_BASE=$$(DEPLOY_BASE)


# =======================================================================
# Fill defaults for unset

isEmpty(DEPLOY_BASE) : DEPLOY_BASE=$$OUT_PWD/deploy

# =======================================================================
# Set compiler flags and paths

unix:!macx {
    isEmpty(GIT_PATH) : GIT_PATH=git
    # =======================================================================
    # Detect Raspberry platform
    RASPBERRY = $$system(bash $$PWD/onrpi)
    !isEmpty(RASPBERRY) {
        message(Raspberry Hardware Detected)
        DEFINES+= Q_OS_RPI
        LIBS += -lpigpio
    }
}

win32 {
    WINDEPLOY_FLAGS = --compiler-runtime --no-system-d3d-compiler --no-opengl --no-opengl-sw --no-angle

    CONFIG(debug, debug|release) : WINDEPLOY_FLAGS += --debug
    CONFIG(release, debug|release) : WINDEPLOY_FLAGS += --release

    DEFINES += _WINSOCKAPI_
    LIBS += -lWs2_32
    LIBS += -liphlpapi
    LIBS += -luser32
    LIBS += -lole32
}

macx {
    # Mac Specific
    # Compatibility down to OS X 10.10
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.10
    QT_CONFIG -= no-pkg-config

    LIBS += -framework CoreFoundation
    isEmpty(GIT_PATH) : GIT_PATH=git
}

# =======================================================================
# Print values when running qmake
!isEqual(QUIET, "true") {
message(-----------------------------------)
message(VERSION_MAJOR: $$VERSION_MAJOR)
message(VERSION_MINOR: $$VERSION_MINOR)
message(VERSION_BUILD: $$VERSION_BUILD)
message(DEPLOY_BASE: $$DEPLOY_BASE)
message(DEFINES: $$DEFINES)
message(INCLUDEPATH: $$INCLUDEPATH)
message(LIBS: $$LIBS)
message(TARGET_NAME: $$TARGET_NAME)
message(QT_INSTALL_PREFIX: $$[QT_INSTALL_PREFIX])
message(QT_INSTALL_LIBS: $$[QT_INSTALL_LIBS])
message(QT_INSTALL_PLUGINS: $$[QT_INSTALL_PLUGINS])
message(QT_INSTALL_TRANSLATIONS: $$[QT_INSTALL_TRANSLATIONS])
message(QT_INSTALL_BINS: $$[QT_INSTALL_BINS])
message(CONFIG: $$CONFIG)
message(-----------------------------------)
}

# =====================================================================
# Files

SOURCES += \
        CRCenc.cpp \
        DMRData.cpp \
        Golay24128.cpp \
        SHA256.cpp \
        YSFConvolution.cpp \
        YSFFICH.cpp \
        ambe.c \
        ambe3600x2400.c \
        ambe3600x2450.c \
        audioengine.cpp \
        cbptc19696.cpp \
        cgolay2087.cpp \
        chamming.cpp \
        codec2/codebooks.cpp \
        codec2/codec2.cpp \
        codec2/kiss_fft.cpp \
        codec2/lpc.cpp \
        codec2/nlp.cpp \
        codec2/pack.cpp \
        codec2/qbase.cpp \
        codec2/quantise.cpp \
        crs129.cpp \
        dcscodec.cpp \
        dmrcodec.cpp \
        dudeshield.cpp \
        ecc.c \
        httpmanager.cpp \
        iaxcodec.cpp \
        imbe7200x4400.c \
        imbe_vocoder/aux_sub.cc \
        imbe_vocoder/basicop2.cc \
        imbe_vocoder/ch_decode.cc \
        imbe_vocoder/ch_encode.cc \
        imbe_vocoder/dc_rmv.cc \
        imbe_vocoder/decode.cc \
        imbe_vocoder/dsp_sub.cc \
        imbe_vocoder/encode.cc \
        imbe_vocoder/imbe_vocoder.cc \
        imbe_vocoder/math_sub.cc \
        imbe_vocoder/pe_lpf.cc \
        imbe_vocoder/pitch_est.cc \
        imbe_vocoder/pitch_ref.cc \
        imbe_vocoder/qnt_sub.cc \
        imbe_vocoder/rand_gen.cc \
        imbe_vocoder/sa_decode.cc \
        imbe_vocoder/sa_encode.cc \
        imbe_vocoder/sa_enh.cc \
        imbe_vocoder/tbls.cc \
        imbe_vocoder/uv_synt.cc \
        imbe_vocoder/v_synt.cc \
        imbe_vocoder/v_uv_det.cc \
        levelmeter.cpp \
        m17codec.cpp \
        main.cpp \
        mbedec.cpp \
        mbeenc.cc \
        mbelib.c \
        nxdncodec.cpp \
        p25codec.cpp \
        refcodec.cpp \
        serialambe.cpp \
        tools/clogger.cpp \
        xrfcodec.cpp \
        ysfcodec.cpp

macx {
SOURCES += \
    tools/keepalive.mm
}

HEADERS += \
        CRCenc.h \
        DMRData.h \
        DMRDefines.h \
        Golay24128.h \
        SHA256.h \
        YSFConvolution.h \
        YSFDefines.h \
        YSFFICH.h \
        ambe.h \
        ambe3600x2400_const.h \
        ambe3600x2450_const.h \
        audioengine.h \
        cbptc19696.h \
        cgolay2087.h \
        chamming.h \
        codec2/codec2.h \
        codec2/codec2_internal.h \
        codec2/defines.h \
        codec2/kiss_fft.h \
        codec2/lpc.h \
        codec2/nlp.h \
        codec2/qbase.h \
        codec2/quantise.h \
        crs129.h \
        dcscodec.h \
        dmrcodec.h \
        dudeshield.h \
        dudeshield.rc \
        ecc_const.h \
        httpmanager.h \
        iaxcodec.h \
        iaxdefines.h \
        imbe7200x4400_const.h \
        imbe_vocoder/aux_sub.h \
        imbe_vocoder/basic_op.h \
        imbe_vocoder/ch_decode.h \
        imbe_vocoder/ch_encode.h \
        imbe_vocoder/dc_rmv.h \
        imbe_vocoder/decode.h \
        imbe_vocoder/dsp_sub.h \
        imbe_vocoder/encode.h \
        imbe_vocoder/globals.h \
        imbe_vocoder/imbe.h \
        imbe_vocoder/imbe_vocoder.h \
        imbe_vocoder/math_sub.h \
        imbe_vocoder/pe_lpf.h \
        imbe_vocoder/pitch_est.h \
        imbe_vocoder/pitch_ref.h \
        imbe_vocoder/qnt_sub.h \
        imbe_vocoder/rand_gen.h \
        imbe_vocoder/sa_decode.h \
        imbe_vocoder/sa_encode.h \
        imbe_vocoder/sa_enh.h \
        imbe_vocoder/tbls.h \
        imbe_vocoder/typedef.h \
        imbe_vocoder/typedefs.h \
        imbe_vocoder/uv_synt.h \
        imbe_vocoder/v_synt.h \
        imbe_vocoder/v_uv_det.h \
        levelmeter.h \
        m17codec.h \
        mbedec.h \
        mbeenc.h \
        mbelib.h \
        mbelib_const.h \
        mbelib_parms.h \
        nxdncodec.h \
        p25codec.h \
        refcodec.h \
        serialambe.h \
        tools/call_once.h \
        tools/clogger.h \
        tools/singleton.h \
        tools/version.h \
        vocoder_tables.h \
        xrfcodec.h \
        ysfcodec.h

macx {
HEADERS += \
    tools/keepalive.h
}

FORMS += \
        dudeshield.ui


contains(DEFINES, USE_FLITE){
    LIBS += -lflite_cmu_us_slt -lflite_cmu_us_kal16 -lflite_cmu_us_awb -lflite_cmu_us_rms -lflite_usenglish -lflite_cmulex -lflite -lasound
}
RC_ICONS = images/dudeshield.ico

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    dudeshield.qrc

ICON = images/dudeshield.png

win32 {
    RC_FILE = dudeshield.rc
}

OTHER_FILES += \
    README.md

win32 {
    OTHER_FILES += dudeshield.iss
}

# =====================================================================
# Local deployment commands for development
unix:!macx {
    copydata.commands += mkdir -p $$OUT_PWD/translations &&
    copydata.commands += cp -avfu $$PWD/translations/*.qm $$OUT_PWD/translations
}

# Mac OS X - Copy help and Marble plugins and data
macx {
    copydata.commands += cp -vf $$PWD/translations/*.qm $$OUT_PWD/dudeshield.app/Contents/Resources
}

#Windows - Copy Qt Style and translation
win32 {
    defineReplace(p){return ($$shell_quote($$shell_path($$1)))}
    exists($$p($$OUT_PWD/translations)) {
        message("existing")
    } else {
        copydata.commands = mkdir $$p($$OUT_PWD/translations) &&
    }
    copydata.commands += xcopy /Y $$p($$PWD/translations/*.qm) $$p($$OUT_PWD/translations)
}

# =====================================================================
# Deployment commands

#linux make install
unix:!macx {
    PREFIX = /usr
    Binary.path = $${PREFIX}/bin
    Binary.files = $$OUT_PWD/$${TARGET}
    INSTALLS += Binary

    Translation.path = $${PREFIX}/share/$${TARGET}/translations
    Translation.files = $$PWD/translations/*.qm
    INSTALLS += Translation

    Icons.path = $${PREFIX}/share/icons/hicolor/72x72/apps
    Icons.files = $$PWD/images/dudeshield.png
    INSTALLS += Icons

    Shortcut.path = $${PREFIX}/share/applications
    Shortcut.files = $$PWD/*.desktop
    INSTALLS += Shortcut
}

# Linux specific deploy target
unix:!macx {
    DEPLOY_DIR=\"$$DEPLOY_BASE/$$TARGET_NAME\"
    DEPLOY_DIR_LIB=\"$$DEPLOY_BASE/$$TARGET_NAME/lib\"
    message(-----------------------------------)
    message(DEPLOY_DIR: $$DEPLOY_DIR)
    message(-----------------------------------)
    deploy.commands += rm -Rfv $$DEPLOY_DIR &&
    deploy.commands += mkdir -pv $$DEPLOY_DIR_LIB &&
    deploy.commands += mkdir -pv $$DEPLOY_DIR_LIB/iconengines &&
    deploy.commands += mkdir -pv $$DEPLOY_DIR_LIB/imageformats &&
    deploy.commands += mkdir -pv $$DEPLOY_DIR_LIB/platforms &&
    deploy.commands += mkdir -pv $$DEPLOY_DIR_LIB/platformthemes &&
    deploy.commands += mkdir -pv $$DEPLOY_DIR_LIB/printsupport &&
    deploy.commands += cp -Rvf $$OUT_PWD/translations $$DEPLOY_DIR &&
    deploy.commands += cp -vfa $$[QT_INSTALL_TRANSLATIONS]/qt_??.qm  $$DEPLOY_DIR/translations &&
    deploy.commands += cp -vfa $$[QT_INSTALL_TRANSLATIONS]/qt_??_??.qm  $$DEPLOY_DIR/translations &&
    deploy.commands += cp -vfa $$[QT_INSTALL_TRANSLATIONS]/qtbase*.qm  $$DEPLOY_DIR/translations &&
    deploy.commands += cp -vf $$PWD/desktop/qt.conf $$DEPLOY_DIR &&
    deploy.commands += cp -vf $$PWD/README.md $$DEPLOY_DIR &&
    deploy.commands += cp -vf $$PWD/gpl-2.0.md $$DEPLOY_DIR &&
    deploy.commands += cp -vfa $$[QT_INSTALL_PLUGINS]/iconengines/libqsvgicon.so*  $$DEPLOY_DIR_LIB/iconengines &&
    deploy.commands += cp -vfa $$[QT_INSTALL_PLUGINS]/imageformats/libqgif.so*  $$DEPLOY_DIR_LIB/imageformats &&
    deploy.commands += cp -vfa $$[QT_INSTALL_PLUGINS]/imageformats/libqjpeg.so*  $$DEPLOY_DIR_LIB/imageformats &&
    deploy.commands += cp -vfa $$[QT_INSTALL_PLUGINS]/imageformats/libqsvg.so*  $$DEPLOY_DIR_LIB/imageformats &&
    deploy.commands += cp -vfa $$[QT_INSTALL_PLUGINS]/imageformats/libqwbmp.so*  $$DEPLOY_DIR_LIB/imageformats &&
    deploy.commands += cp -vfa $$[QT_INSTALL_PLUGINS]/imageformats/libqwebp.so*  $$DEPLOY_DIR_LIB/imageformats &&
    deploy.commands += cp -vfa $$[QT_INSTALL_PLUGINS]/platforms/libqeglfs.so*  $$DEPLOY_DIR_LIB/platforms &&
    deploy.commands += cp -vfa $$[QT_INSTALL_PLUGINS]/platforms/libqlinuxfb.so*  $$DEPLOY_DIR_LIB/platforms &&
    deploy.commands += cp -vfa $$[QT_INSTALL_PLUGINS]/platforms/libqminimal.so*  $$DEPLOY_DIR_LIB/platforms &&
    deploy.commands += cp -vfa $$[QT_INSTALL_PLUGINS]/platforms/libqminimalegl.so*  $$DEPLOY_DIR_LIB/platforms &&
    deploy.commands += cp -vfa $$[QT_INSTALL_PLUGINS]/platforms/libqoffscreen.so*  $$DEPLOY_DIR_LIB/platforms &&
    deploy.commands += cp -vfa $$[QT_INSTALL_PLUGINS]/platforms/libqxcb.so*  $$DEPLOY_DIR_LIB/platforms &&
    deploy.commands += cp -vfa $$[QT_INSTALL_PLUGINS]/platformthemes/libqgtk*.so*  $$DEPLOY_DIR_LIB/platformthemes &&
    deploy.commands += cp -vfa $$[QT_INSTALL_LIBS]/libicudata.so*  $$DEPLOY_DIR_LIB &&
    deploy.commands += cp -vfa $$[QT_INSTALL_LIBS]/libicui18n.so*  $$DEPLOY_DIR_LIB &&
    deploy.commands += cp -vfa $$[QT_INSTALL_LIBS]/libicuuc.so*  $$DEPLOY_DIR_LIB &&
    deploy.commands += cp -vfa $$[QT_INSTALL_LIBS]/libQt5Concurrent.so*  $$DEPLOY_DIR_LIB &&
    deploy.commands += cp -vfa $$[QT_INSTALL_LIBS]/libQt5Core.so*  $$DEPLOY_DIR_LIB &&
    deploy.commands += cp -vfa $$[QT_INSTALL_LIBS]/libQt5DBus.so*  $$DEPLOY_DIR_LIB &&
    deploy.commands += cp -vfa $$[QT_INSTALL_LIBS]/libQt5Gui.so*  $$DEPLOY_DIR_LIB &&
    deploy.commands += cp -vfa $$[QT_INSTALL_LIBS]/libQt5Network.so*  $$DEPLOY_DIR_LIB &&
    deploy.commands += cp -vfa $$[QT_INSTALL_LIBS]/libQt5Qml.so*  $$DEPLOY_DIR_LIB &&
    deploy.commands += cp -vfa $$[QT_INSTALL_LIBS]/libQt5Quick.so*  $$DEPLOY_DIR_LIB &&
    deploy.commands += cp -vfa $$[QT_INSTALL_LIBS]/libQt5Script.so*  $$DEPLOY_DIR_LIB &&
    deploy.commands += cp -vfa $$[QT_INSTALL_LIBS]/libQt5Sql.so*  $$DEPLOY_DIR_LIB &&
    deploy.commands += cp -vfa $$[QT_INSTALL_LIBS]/libQt5Svg.so*  $$DEPLOY_DIR_LIB &&
    deploy.commands += cp -vfa $$[QT_INSTALL_LIBS]/libQt5Widgets.so*  $$DEPLOY_DIR_LIB &&
    deploy.commands += cp -vfa $$[QT_INSTALL_LIBS]/libQt5X11Extras.so*  $$DEPLOY_DIR_LIB &&
    deploy.commands += cp -vfa $$[QT_INSTALL_LIBS]/libQt5XcbQpa.so*  $$DEPLOY_DIR_LIB &&
    deploy.commands += cp -vfa $$[QT_INSTALL_LIBS]/libQt5Xml.so* $$DEPLOY_DIR_LIB
}

# Mac specific deploy target
macx {
    DEPLOY_DIR=\"$$PWD/../deploy\"
    message(-----------------------------------)
    message(DEPLOY_DIR: $$DEPLOY_DIR)
    message(-----------------------------------)
    deploy.commands += rm -Rfv $$DEPLOY_DIR/* &&
    !exists($$DEPLOY_DIR) : deploy.commands += mkdir -p $$DEPLOY_DIR &&
    deploy.commands += mkdir -p $$OUT_PWD/dudeshield.app/Contents/PlugIns &&
    deploy.commands += cp -fv $$[QT_INSTALL_TRANSLATIONS]/qt_??.qm  $$OUT_PWD/dudeshield.app/Contents/Resources &&
    deploy.commands += cp -fv $$[QT_INSTALL_TRANSLATIONS]/qt_??_??.qm  $$OUT_PWD/dudeshield.app/Contents/Resources &&
    deploy.commands += cp -fv $$[QT_INSTALL_TRANSLATIONS]/qtbase*.qm  $$OUT_PWD/dudeshield.app/Contents/Resources &&
    deploy.commands += $$[QT_INSTALL_PREFIX]/bin/macdeployqt dudeshield.app -always-overwrite -dmg &&
    deploy.commands += cp -fv $$OUT_PWD/dudeshield.dmg $$DEPLOY_DIR/dudeshield_$${VERSION_MAJOR}_$${VERSION_MINOR}_$${VERSION_BUILD}_mac.dmg
}

# Windows specific deploy target
win32 {
    defineReplace(p){return ($$shell_quote($$shell_path($$1)))}
    RC_ICONS = $$p($$PWD/images/dudeshield.ico)
    CONFIG(debug, debug|release) : DLL_SUFFIX=d
    CONFIG(release, debug|release) : DLL_SUFFIX=
    deploy.commands = ( rmdir /s /q $$p($$DEPLOY_BASE) || echo Directory already empty) &&
    deploy.commands += mkdir $$p($$DEPLOY_BASE/$$TARGET_NAME/translations) &&
    deploy.commands += xcopy /Y $$p($$OUT_PWD/$$TARGET.$$TARGET_EXT) $$p($$DEPLOY_BASE/$$TARGET_NAME) &&
    deploy.commands += xcopy /Y $$p($$PWD/README.md) $$p($$DEPLOY_BASE/$$TARGET_NAME) &&
    deploy.commands += xcopy /Y $$p($$PWD/translations/*.qm) $$p($$DEPLOY_BASE/$$TARGET_NAME/translations) &&
    #deploy.commands += xcopy /Y $$p($$PWD/*.txt) $$p($$DEPLOY_BASE/$$TARGET_NAME) &&
    deploy.commands += xcopy /Y $$p($$PWD/gpl-2.0.md) $$p($$DEPLOY_BASE/$$TARGET_NAME) &&
    deploy.commands += xcopy /Y $$p($$PWD/dudeshield.iss) $$p($$DEPLOY_BASE/$$TARGET_NAME) &&
    deploy.commands += xcopy /Y $$p($$PWD/images/dudeshield.ico) $$p($$DEPLOY_BASE/$$TARGET_NAME) &&
    deploy.commands += $$p($$[QT_INSTALL_BINS]/windeployqt) $$WINDEPLOY_FLAGS $$p($$DEPLOY_BASE/$$TARGET_NAME) &&
    deploy.commands += compil32 /cc $$p($$DEPLOY_BASE/$$TARGET_NAME/dudeshield.iss)
}

# =====================================================================
# Additional targets

# Need to copy data when compiling
all.depends = copydata

# Deploy needs compiling before
deploy.depends = all

QMAKE_EXTRA_TARGETS += deploy copydata all

DISTFILES += \
    gpl-2.0.md \
    onrpi

