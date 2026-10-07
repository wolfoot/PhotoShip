#include "psd.h"
#include <QColorSpace>
#include <QFile>
#include <QMap>
#include <stdexcept>
#include <zlib.h>
namespace ps {
namespace {
struct Reader {
    QByteArray bytes;
    qint64 at = 0;
    qint64 left() const {
        return bytes.size() - at;
    }
    QByteArray take(qint64 n) {
        if (n < 0 || n > left())
            throw std::runtime_error("Truncated PSD section or invalid length.");
        QByteArray v = bytes.mid(at, n);
        at += n;
        return v;
    }
    quint8 u8() {
        return quint8(take(1)[0]);
    }
    quint16 u16() {
        auto b = take(2);
        return (quint16(quint8(b[0])) << 8) | quint8(b[1]);
    }
    quint32 u32() {
        auto b = take(4);
        return (quint32(quint8(b[0])) << 24) | (quint32(quint8(b[1])) << 16) | (quint32(quint8(b[2])) << 8) |
               quint8(b[3]);
    }
    qint32 i32() {
        return qint32(u32());
    }
    Reader section() {
        return {take(u32())};
    }
    QRect bounds() {
        int t = i32(), l = i32(), b = i32(), r = i32();
        if (std::abs(qint64(t)) > 1000000 || std::abs(qint64(l)) > 1000000 || std::abs(qint64(b)) > 1000000 ||
            std::abs(qint64(r)) > 1000000 || r < l || b < t)
            throw std::runtime_error("Invalid PSD layer bounds.");
        return QRect(l, t, r - l, b - t);
    }
};
QByteArray unpack(Reader &r, int w, int h, int compression) {
    qint64 count = qint64(w) * h;
    if (count < 0 || count > MaxSourcePixels)
        throw std::runtime_error("PSD channel exceeds pixel limit.");
    if (!count)
        return {};
    if (compression == 0)
        return r.take(count);
    if (compression == 1) {
        QVector<int> lengths;
        for (int y = 0; y < h; ++y)
            lengths << r.u16();
        QByteArray out;
        out.reserve(count);
        for (int len : lengths) {
            Reader line{r.take(len)};
            int start = out.size();
            while (line.left()) {
                int n = qint8(line.u8());
                if (n >= 0)
                    out += line.take(n + 1);
                else if (n != -128)
                    out += QByteArray(1 - n, char(line.u8()));
                if (out.size() - start > w)
                    throw std::runtime_error("PSD RLE row overrun.");
            }
            if (out.size() - start != w)
                throw std::runtime_error("PSD RLE row has incorrect length.");
        }
        return out;
    }
    if (compression == 2 || compression == 3) {
        QByteArray out(int(count), 0);
        QByteArray input = r.take(r.left());
        z_stream stream{};
        stream.next_in = reinterpret_cast<Bytef *>(input.data());
        stream.avail_in = input.size();
        stream.next_out = reinterpret_cast<Bytef *>(out.data());
        stream.avail_out = out.size();
        if (inflateInit(&stream) != Z_OK)
            throw std::runtime_error("Cannot initialize ZIP decoder.");
        int status = inflate(&stream, Z_FINISH);
        bool valid = status == Z_STREAM_END && stream.total_out == quint64(count) && stream.avail_in == 0;
        inflateEnd(&stream);
        if (!valid)
            throw std::runtime_error("Invalid or oversized PSD ZIP channel.");
        if (compression == 3)
            for (int y = 0; y < h; ++y)
                for (int x = 1; x < w; ++x)
                    out[y * w + x] = char(quint8(out[y * w + x]) + quint8(out[y * w + x - 1]));
        return out;
    }

    throw std::runtime_error("Unsupported PSD compression.");
}
QImage rgb(const QMap<int, QByteArray> &channels, QSize size) {
    if (!channels.contains(0) || !channels.contains(1) || !channels.contains(2))
        return {};
    QImage image(size, QImage::Format_ARGB32);
    for (int y = 0; y < size.height(); ++y) {
        auto row = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < size.width(); ++x) {
            int i = y * size.width() + x;
            row[x] = qRgba(quint8(channels[0][i]), quint8(channels[1][i]), quint8(channels[2][i]),
                           channels.contains(-1) ? quint8(channels[-1][i]) : 255);
        }
    }
    image.setColorSpace(QColorSpace::SRgb);
    return image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}
struct Record {
    Layer layer;
    QRect bounds, maskBounds;
    int maskDefault = 255, maskFlags = 0, section = 0;
    QVector<QPair<int, quint32>> channels;
};
QString blendName(QByteArray key) {
    static QMap<QByteArray, QString> map{
        {"norm", "Normal"},     {"pass", "Normal"},     {"mul ", "Multiply"},   {"scrn", "Screen"},
        {"over", "Overlay"},    {"dark", "Darken"},     {"lite", "Lighten"},    {"div ", "Color Dodge"},
        {"idiv", "Color Burn"}, {"hLit", "Hard Light"}, {"sLit", "Soft Light"}, {"diff", "Difference"},
        {"smud", "Exclusion"}};
    return map.value(key);
}
} // namespace
bool readPsd(const QString &path, State &result, QStringList &report, QString &error) {
    report.clear();
    try {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            error = file.errorString();
            return false;
        }
        if (file.size() > 256LL * 1024 * 1024)
            throw std::runtime_error("PSD file exceeds the 256 MiB import limit.");
        Reader r{file.readAll()};
        if (r.take(4) != "8BPS" || r.u16() != 1)
            throw std::runtime_error("Only PSD v1 is supported (PSB is not supported).");
        if (r.take(6) != QByteArray(6, 0))
            throw std::runtime_error("Invalid PSD reserved bytes.");
        int channelCount = r.u16();
        int h = r.u32(), w = r.u32();
        int depth = r.u16(), mode = r.u16();
        if (depth != 8 || mode != 3 || channelCount < 3 || channelCount > 56)
            throw std::runtime_error(
                "Only 8-bit RGB PSD is supported. Convert the document to RGB/8-bit first.");
        State s;
        s.size = {w, h};
        if (!Document::validSize(s.size))
            throw std::runtime_error("PSD canvas exceeds the 16 million pixel / 8192 side limit.");
        r.section();
        Reader resources = r.section();
        QColorSpace profile = QColorSpace::SRgb;
        while (resources.left()) {
            if (resources.take(4) != "8BIM")
                throw std::runtime_error("Invalid PSD resource signature.");
            int id = resources.u16(), name = resources.u8();
            resources.take(name);
            if ((name + 1) % 2)
                resources.take(1);
            Reader data = resources.section();
            if (data.bytes.size() % 2)
                resources.take(1);
            if (id == 1039) {
                profile = QColorSpace::fromIccProfile(data.bytes);
                if (!profile.isValid()) {
                    profile = QColorSpace::SRgb;
                    report << "Invalid ICC profile; assigned sRGB.";
                }
            }
            if (id == 1005 && data.left() >= 16) {
                double dpi = data.u32() / 65536.;
                if (dpi >= 1 && dpi <= 9600)
                    s.resolution = dpi;
            }
            if (id == 1032 || id == 1022 || id == 1025)
                report << "Guides/spot channels are not imported.";
        }
        Reader lm = r.section();
        QVector<Record> records;
        bool mergedAlpha = false;
        qint64 pixelBudget = 0, maskBudget = 0;
        if (lm.left() >= 4) {
            Reader info = lm.section();
            if (info.left()) {
                int signedCount = qint16(info.u16());
                mergedAlpha = signedCount < 0;
                int n = std::abs(signedCount);
                if (n > MaxLayers)
                    throw std::runtime_error("PSD has too many layers.");
                for (int i = 0; i < n; ++i) {
                    Record rec;
                    rec.bounds = info.bounds();
                    int channels = info.u16();
                    if (channels > 56)
                        throw std::runtime_error("PSD layer has too many channels.");
                    for (int j = 0; j < channels; ++j) {
                        int id = qint16(info.u16());
                        quint32 length = info.u32();
                        rec.channels << qMakePair(id, length);
                    }
                    if (info.take(4) != "8BIM")
                        throw std::runtime_error("Invalid PSD blend signature.");
                    auto key = info.take(4);
                    rec.layer.blend = blendName(key);
                    if (rec.layer.blend.isEmpty()) {
                        rec.layer.blend = "Normal";
                        report << QString("Layer %1: unsupported blend %2; imported as Normal.")
                                      .arg(i + 1)
                                      .arg(QString::fromLatin1(key));
                    }
                    rec.layer.opacity = info.u8() / 255.;
                    rec.layer.clipped = info.u8() != 0;
                    int flags = info.u8();
                    rec.layer.visible = !(flags & 2);
                    info.u8();
                    Reader extra = info.section();
                    Reader mask = extra.section();
                    if (mask.left()) {
                        if (mask.left() < 18)
                            throw std::runtime_error("Invalid PSD mask record.");
                        rec.maskBounds = mask.bounds();
                        rec.maskDefault = mask.u8();
                        rec.maskFlags = mask.u8();
                        if (rec.maskFlags & 1)
                            rec.maskBounds.translate(rec.bounds.topLeft());
                        if (mask.left() > 2)
                            report << QString("Layer %1: mask density/feather or vector parameters are not "
                                              "preserved.")
                                          .arg(i + 1);
                    }
                    Reader ranges = extra.section();
                    if (!ranges.bytes.isEmpty() &&
                        ranges.bytes !=
                            QByteArray::fromHex("0000ffff0000ffff").repeated(ranges.bytes.size() / 8))
                        report << QString("Layer %1: Blend If ranges are not preserved.").arg(i + 1);
                    int name = extra.u8();
                    rec.layer.name = QString::fromLatin1(extra.take(name));
                    extra.take((4 - (name + 1) % 4) % 4);
                    while (extra.left() >= 12) {
                        auto sig = extra.take(4);
                        if (sig != "8BIM" && sig != "8B64")
                            throw std::runtime_error("Invalid PSD additional-info signature.");
                        auto tag = extra.take(4);
                        quint64 length = extra.u32();
                        if (sig == "8B64")
                            length = (length << 32) | extra.u32();
                        if (length > quint64(extra.left()))
                            throw std::runtime_error("Invalid PSD additional-info length.");
                        Reader data{extra.take(length)};
                        if (data.bytes.size() % 2)
                            extra.take(1);
                        if (tag == "iOpa" && data.left()) {
                            int fill = data.u8();
                            rec.layer.opacity *= fill / 255.;
                            if (fill != 255)
                                report << rec.layer.name + ": fill opacity folded into layer opacity; "
                                                           "non-Normal blending may differ.";
                        } else if (tag == "luni") {
                            quint32 count = data.u32();
                            if (count > 4096 || qint64(count) * 2 > data.left())
                                throw std::runtime_error("PSD layer name too long.");
                            QString name;
                            for (quint32 j = 0; j < count; ++j)
                                name += QChar(data.u16());
                            rec.layer.name = name;
                        } else if (tag == "lsct" || tag == "lsdk") {
                            rec.section = data.u32();
                            if (data.left() >= 8) {
                                data.take(4);
                                auto mode = data.take(4);
                                if (mode != "pass")
                                    report << rec.layer.name + ": isolated/blended group imported as "
                                                               "pass-through; its appearance can differ.";
                            }
                        } else if (QList<QByteArray>{"TySh", "SoLd", "SoLE", "vmsk", "vsms", "lfx2", "lrFX",
                                                     "levl", "curv", "hue2", "brit", "blnc", "selc", "mixr",
                                                     "grdm", "nvrt", "expA", "vibA", "knko", "brst", "FMsk",
                                                     "vstk", "vscg", "vogk", "blwh", "PtFl", "GdFl", "SoCo"}
                                       .contains(tag))
                            report << rec.layer.name + ": " + QString::fromLatin1(tag) +
                                          " uses cached pixels; editable "
                                          "effects/text/vector/smart-object/adjustment parameters are not "
                                          "preserved.";
                    }
                    if (rec.section == 1 || rec.section == 2) {
                        if (rec.layer.clipped)
                            report << rec.layer.name + ": group clipping is not supported.";
                        rec.layer.kind = "group";
                        rec.layer.size = s.size;
                        rec.layer.clipped = false;
                        rec.layer.blend = "Normal";
                        if (!rec.maskBounds.isEmpty())
                            report << rec.layer.name + ": group mask is not preserved.";
                    } else if (rec.section != 3 && !rec.bounds.isEmpty()) {
                        if (!Document::validSize(rec.bounds.size(), MaxSourcePixels))
                            throw std::runtime_error("PSD layer exceeds source limits.");
                        pixelBudget += qint64(rec.bounds.width()) * rec.bounds.height();
                        if (pixelBudget > MaxSourcePixels)
                            throw std::runtime_error("PSD exceeds total 32 million source pixels.");
                        rec.layer.position = rec.bounds.topLeft();
                        rec.layer.size = rec.bounds.size();
                    }
                    records << rec;
                }
                for (auto &rec : records) {
                    QMap<int, QByteArray> planes;
                    for (auto channel : rec.channels) {
                        Reader data{info.take(channel.second)};
                        int compression = data.u16();
                        QSize size = channel.first == -2 ? rec.maskBounds.size() : rec.bounds.size();
                        if (channel.first >= -2 && channel.first <= 2 && !size.isEmpty()) {
                            if (!Document::validSize(size, MaxSourcePixels))
                                throw std::runtime_error("Invalid PSD mask/channel dimensions.");
                            planes[channel.first] = unpack(data, size.width(), size.height(), compression);
                            if (data.left())
                                throw std::runtime_error("Extra bytes in PSD channel.");
                        } else if (channel.first != -3 && channel.first > 2)
                            report << rec.layer.name + ": extra channels omitted.";
                    }
                    if (rec.section || rec.bounds.isEmpty())
                        continue;
                    rec.layer.image = rgb(planes, rec.bounds.size());
                    if (rec.layer.image.isNull()) {
                        report << rec.layer.name + ": no RGB cache; layer omitted.";
                        continue;
                    }
                    if (profile.isValid() && profile != QColorSpace(QColorSpace::SRgb)) {
                        rec.layer.image.setColorSpace(profile);
                        rec.layer.image = rec.layer.image.convertedToColorSpace(QColorSpace::SRgb);
                    }
                    if (planes.contains(-2)) {
                        maskBudget += qint64(rec.bounds.width()) * rec.bounds.height();
                        if (maskBudget > MaxSourcePixels)
                            throw std::runtime_error("PSD mask budget exceeded.");
                        rec.layer.mask = QImage(rec.bounds.size(), QImage::Format_ARGB32_Premultiplied);
                        int defaultValue = (rec.maskFlags & 4) ? 255 - rec.maskDefault : rec.maskDefault;
                        rec.layer.mask.fill(QColor(defaultValue, defaultValue, defaultValue));
                        QRect overlap = rec.bounds.intersected(rec.maskBounds);
                        for (int y = overlap.top(); y <= overlap.bottom(); ++y)
                            for (int x = overlap.left(); x <= overlap.right(); ++x) {
                                int v = quint8(planes[-2][(y - rec.maskBounds.y()) * rec.maskBounds.width() +
                                                          x - rec.maskBounds.x()]);
                                if (rec.maskFlags & 4)
                                    v = 255 - v;
                                rec.layer.mask.setPixel(x - rec.bounds.x(), y - rec.bounds.y(),
                                                        qRgb(v, v, v));
                            }
                        rec.layer.maskEnabled = !(rec.maskFlags & 2);
                    }
                }
            }
        }
        // PSD file records are bottom to top, with divider/children/folder.
        // Traverse in reverse to assign parents, then emit contiguous bottom-to-top subtrees.
        QMap<QString, QVector<Layer>> children;
        QStringList stack;
        for (auto it = records.crbegin(); it != records.crend(); ++it) {
            const auto &rec = *it;
            if (rec.section == 3) {
                if (stack.isEmpty())
                    throw std::runtime_error("Unbalanced PSD group markers.");
                stack.removeLast();
                continue;
            }
            if (rec.section == 1 || rec.section == 2) {
                Layer group = rec.layer;
                group.parent = stack.value(stack.size() - 1);
                children[group.parent] << group;
                stack << group.id;
            } else if (!rec.layer.image.isNull()) {
                Layer layer = rec.layer;
                layer.parent = stack.value(stack.size() - 1);
                children[layer.parent] << layer;
            }
        }
        if (!stack.isEmpty())
            throw std::runtime_error("Unclosed PSD group.");
        std::function<void(QString)> append = [&](QString parent) {
            auto list = children.value(parent);
            for (int i = list.size() - 1; i >= 0; --i) {
                s.layers << list[i];
                if (list[i].isGroup())
                    append(list[i].id);
            }
        };
        append({});
        bool hasRaster = false;
        for (const auto &l : s.layers)
            hasRaster = hasRaster || l.kind == "raster";
        if (!hasRaster) {
            s.layers.clear();
            int compression = r.u16();
            QMap<int, QByteArray> channels;
            if (compression == 0) {
                for (int i = 0; i < channelCount; ++i) {
                    auto data = r.take(qint64(w) * h);
                    if (i < 3 || (i == 3 && mergedAlpha))
                        channels[i == 3 ? -1 : i] = data;
                }
            } else if (compression == 1) {
                QVector<int> lengths;
                for (int i = 0; i < channelCount * h; ++i)
                    lengths << r.u16();
                for (int i = 0; i < channelCount; ++i) {
                    QByteArray bytes;
                    for (int y = 0; y < h; ++y) {
                        int length = lengths[i * h + y];
                        bytes += char(length >> 8);
                        bytes += char(length & 255);
                    }
                    for (int y = 0; y < h; ++y)
                        bytes += r.take(lengths[i * h + y]);
                    Reader rows{bytes};
                    auto data = unpack(rows, w, h, 1);
                    if (i < 3 || (i == 3 && mergedAlpha))
                        channels[i == 3 ? -1 : i] = data;
                }
            } else
                throw std::runtime_error("Flattened PSD fallback supports raw/RLE compression.");
            Layer layer;
            layer.name = "PSD merged image";
            layer.size = s.size;
            layer.image = rgb(channels, s.size);
            if (profile != QColorSpace(QColorSpace::SRgb)) {
                layer.image.setColorSpace(profile);
                layer.image = layer.image.convertedToColorSpace(QColorSpace::SRgb);
            }
            s.layers << layer;
            report << "No usable layers; imported the merged image.";
        }
        s.active = s.layers.last().id;
        report.removeDuplicates();
        report.prepend(QString("Imported %1 layers/groups, 8-bit RGB → sRGB. PSD export is unavailable.")
                           .arg(s.layers.size()));
        result = std::move(s);
        return true;
    } catch (const std::exception &e) {
        error = QString::fromUtf8(e.what());
        return false;
    }
}
} // namespace ps
