/****************************************************************************
**
** Regression test for the loongarch64 VNC pixel conversion fix.
**
** QVNCServer::convertPixels() used to compile its fast 16bpp -> 32bpp (5-6-5)
** conversion path only on x86.  On loongarch64 the fallback path was taken
** instead, which produced wrong pixels and, in the RFB hextile encoder,
** overran the destination buffer (symptom: QPE segfaulted in memmove from
** QVNCServer::convertPixels as soon as a VNC client connected).
**
** This test:
**   1. FAILS TO COMPILE if the fast path is not enabled on loongarch64.
**   2. Checks the fast path produces the same pixels as qt_conv16ToRgb().
**   3. Exercises even and odd pixel counts (the trailing-pixel branch).
**   4. Uses guard canaries around the destination to catch buffer overruns.
**
****************************************************************************/

#include "qscreenvnc_p.h"

#include <QtCore/QDebug>
#include <QtCore/QVector>

// The build must enable the fast conversion path on loongarch64.  If someone
// removes __loongarch64__ from the #if in qscreenvnc_p.h, this test stops
// compiling -- which is the point of the regression test.
#if defined(__loongarch__) && !defined(QT_QWS_VNC_FAST_CONV16TO32)
#error "LoongArch VNC fast 16->32 conversion is disabled (regression)"
#endif

#if !defined(QT_QWS_VNC_FAST_CONV16TO32)
#error "VNC fast 16->32 conversion helper is not available"
#endif

static int failures = 0;

#define CHECK(cond, what) do { \
    if (!(cond)) { \
        qWarning("FAIL: %s (line %d)", what, __LINE__); \
        ++failures; \
    } \
} while (0)

static void checkConversion(const quint16 *src, int count, const char *name)
{
    // dst mimics QRfbMultiColorHextile::rect(): 4-byte aligned, exactly
    // count*4 bytes, surrounded by canaries to catch the overrun.
    const quint32 guard = 0xdeadbeefu;
    QVector<quint32> buf(count + 2);
    buf[0] = guard;
    buf[count + 1] = guard;
    for (int i = 0; i < count; ++i)
        buf[i + 1] = 0;

    qt_vnc_conv16To32(buf.data() + 1, reinterpret_cast<const char *>(src), count);

    CHECK(buf[0] == guard, "underflow before dst buffer");
    CHECK(buf[count + 1] == guard, "overflow after dst buffer");
    for (int i = 0; i < count; ++i) {
        // RFB 32bpp is XRGB (alpha byte unused); qt_conv16ToRgb() sets alpha.
        const quint32 expected = qt_conv16ToRgb(src[i]) & 0x00ffffffu;
        const quint32 got = buf[i + 1];
        // The hand-optimized loop and qt_conv16ToRgb() use slightly different
        // rounding for 5/6-bit channels (up to one 5-bit step ~= 8).  Allow
        // that, but still catch wrong pixels or swapped channels.
        const int dr = int((got >> 16) & 0xff) - int((expected >> 16) & 0xff);
        const int dg = int((got >> 8) & 0xff) - int((expected >> 8) & 0xff);
        const int db = int(got & 0xff) - int(expected & 0xff);
        if (dr > 8 || dr < -8 || dg > 8 || dg < -8 || db > 8 || db < -8) {
            qWarning("FAIL: %s pixel %d: got 0x%06x, expected 0x%06x (src 0x%04x)",
                     name, i, got, expected, src[i]);
            ++failures;
        }
    }
    if (!failures)
        qDebug("PASS: %s (%d pixels)", name, count);
}

int main(int argc, char **argv)
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    // 5-6-5 edge values plus a mid value.
    const quint16 samples[] = {
        0x0000, // black
        0xffff, // white
        0xf800, // red
        0x07e0, // green
        0x001f, // blue
        0x1234, // mixed
        0xabcd  // mixed
    };
    const int sampleCount = int(sizeof(samples) / sizeof(samples[0]));

    // Even counts exercise the paired (two-pixels-at-a-time) loop.
    checkConversion(samples, 2, "even count (2)");
    checkConversion(samples, 4, "even count (4)");
    checkConversion(samples, 6, "even count (6)");

    // Odd counts exercise the trailing single-pixel branch, which is where
    // the original x86-only code was most fragile.
    checkConversion(samples, 1, "odd count (1)");
    checkConversion(samples, 3, "odd count (3)");
    checkConversion(samples, 5, "odd count (5)");
    checkConversion(samples, sampleCount == 7 ? 7 : sampleCount, "odd count (all samples)");

    if (failures) {
        qWarning("%d VNC pixel conversion check(s) failed", failures);
        return 1;
    }
    qDebug("All VNC pixel conversion checks passed on %s",
#ifdef __loongarch__
           "LoongArch"
#else
           "this architecture"
#endif
           );
    return 0;
}
