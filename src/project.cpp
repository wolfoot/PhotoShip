#include "document.h"
#include <QColorSpace>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <cmath>
namespace ps {
static constexpr qint64 MaxEncodedBytes = 64LL * 1024 * 1024;
static const QString Format = "org.pixelstudio.project";
static bool fail(QString &error, const QString &message) {
    error = message;
    return false;
}
static bool finite(double n, double min, double max) {
    return std::isfinite(n) && n >= min && n <= max;
}
static bool validate(const State &s, QString &error) {
    if (!Document::validSize(s.size) || s.layers.size() > MaxLayers || QUuid(s.id).isNull() ||
        !finite(s.resolution, 1, 9600))
        return fail(error, "Invalid document dimensions, ID, or layer count.");
    QSet<QString> ids;
    QMap<QString, QString> parents;
    QSet<QString> groups;
    qint64 pixels = 0, masks = 0;
    for (const auto &l : s.layers) {
        if (QUuid(l.id).isNull() || ids.contains(l.id) || l.name.size() > 4096)
            return fail(error, "Invalid or duplicate layer ID, or an oversized name.");
        ids.insert(l.id);
        parents[l.id] = l.parent;
        if (l.isGroup())
            groups.insert(l.id);
        if (!QStringList{"raster", "group", "text", "shape", "adjustment"}.contains(l.kind) ||
            !Document::blendModes().contains(l.blend) || !finite(l.opacity, 0, 1) ||
            !finite(l.rotation, -360000, 360000) || !finite(l.position.x(), -1000000, 1000000) ||
            !finite(l.position.y(), -1000000, 1000000) || !finite(l.size.width(), 1, MaxSide) ||
            !finite(l.size.height(), 1, MaxSide))
            return fail(error, "Invalid layer kind, appearance, or transform.");
        if (l.isGroup()) {
            if (!l.image.isNull() || !l.mask.isNull() || l.blend != "Normal" || l.clipped)
                return fail(error, "Group masks and group blend modes are not supported in this version.");
        } else {
            QSize source = l.image.isNull() ? l.size.toSize() : l.image.size();
            if (!Document::validSize(source, MaxSourcePixels))
                return fail(error, "Layer source is too large.");
            if (l.kind != "adjustment")
                pixels += qint64(source.width()) * source.height();
            if (!l.mask.isNull()) {
                if (l.mask.size() != source)
                    return fail(error, "Mask dimensions must match the layer source.");
                masks += qint64(l.mask.width()) * l.mask.height();
            }
        }
        if (l.kind == "adjustment") {
            if (!QStringList{"Levels", "Curves", "Hue / Saturation", "Invert"}.contains(l.adjustment) ||
                !finite(l.first, -180, 255) || !finite(l.second, -100, 100) || !finite(l.third, -100, 255) ||
                l.curve.size() > 256 || !l.image.isNull() || l.clipped || l.blend != "Normal")
                return fail(error, "Invalid adjustment layer.");
            if (l.adjustment == "Levels" &&
                (l.first < 0 || l.first >= l.third || l.second < .1 || l.second > 10))
                return fail(error, "Invalid Levels values.");
            double previous = -1;
            for (const auto &point : l.curve) {
                if (!finite(point.x(), 0, 1) || !finite(point.y(), 0, 1) || point.x() <= previous)
                    return fail(error, "Invalid curve points.");
                previous = point.x();
            }
            if (l.adjustment == "Curves" && l.curve.size() < 2)
                return fail(error, "Missing curve points.");
        }
        if (l.kind == "text" &&
            (l.text.size() > 100000 || l.font.pointSizeF() < 1 || l.font.pointSizeF() > 1000))
            return fail(error, "Invalid text or font size.");
        if (!l.color.isValid() || !QStringList{"rectangle", "ellipse"}.contains(l.shape))
            return fail(error, "Invalid shape or color.");
    }
    if (pixels > MaxSourcePixels || masks > MaxSourcePixels)
        return fail(error, "Document exceeds the 32 million source/mask pixel limit.");
    if (!s.active.isEmpty() && !ids.contains(s.active))
        return fail(error, "The active layer does not exist.");
    // Require contiguous subtrees so ordering, grouping, and compositing agree.
    QStringList stack;
    QSet<QString> closed;
    for (const auto &l : s.layers) {
        if (!l.parent.isEmpty() && !groups.contains(l.parent))
            return fail(error, "Layer parent must reference a group.");
        QString p = l.parent;
        QSet<QString> visited{l.id};
        while (!p.isEmpty()) {
            if (visited.contains(p))
                return fail(error, "Cyclic layer hierarchy.");
            visited.insert(p);
            p = parents.value(p);
        }
        while (!stack.isEmpty() && stack.last() != l.parent)
            closed.insert(stack.takeLast());
        if (!l.parent.isEmpty() && (closed.contains(l.parent) || stack.isEmpty()))
            return fail(error, "Group children must follow their group as a contiguous subtree.");
        if (l.isGroup())
            stack << l.id;
    }
    return true;
}
bool readImage(const QString &path, QImage &result, QString &error) {
    QFileInfo file(path);
    if (!file.isFile() || file.size() > MaxEncodedBytes)
        return fail(error, "Image is missing or exceeds 64 MiB encoded.");
    QImageReader reader(path);
    reader.setAutoTransform(true);
    if (!Document::validSize(reader.size(), MaxSourcePixels))
        return fail(error, "Image dimensions are invalid or exceed the size limit.");
    QImage image = reader.read();
    if (image.isNull())
        return fail(error, "Cannot decode image: " + reader.errorString());
    if (image.colorSpace().isValid() && image.colorSpace() != QColorSpace(QColorSpace::SRgb))
        image = image.convertedToColorSpace(QColorSpace::SRgb);
    if (image.isNull())
        return fail(error, "Cannot convert the image color space.");
    image.setColorSpace(QColorSpace::SRgb);
    result = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    return true;
}
static bool asset(const QDir &root, const QString &name, QImage &image, qint64 &used, QString &error) {
    static QRegularExpression safe("^[A-Za-z0-9.-]+\\.png$");
    const QFileInfo folder(root.filePath("images"));
    const QFileInfo file(root.filePath("images/" + name));
    if (!safe.match(name).hasMatch() || name.contains("..") || folder.isSymLink() || file.isSymLink() ||
        !file.isFile() || file.canonicalPath() != folder.canonicalFilePath())
        return fail(error, "Unsafe or missing project image: " + name);
    QImageReader reader(file.filePath());
    QSize size = reader.size();
    if (!Document::validSize(size, MaxSourcePixels) ||
        used + qint64(size.width()) * size.height() > MaxSourcePixels)
        return fail(error, "Project assets exceed the pixel budget.");
    if (!readImage(file.filePath(), image, error))
        return false;
    used += qint64(image.width()) * image.height();
    return true;
}
bool loadProject(const QString &path, State &result, QString &error) {
    QDir root(QFileInfo(path).isDir() ? path : QFileInfo(path).absolutePath());
    QFile file(root.filePath("manifest.json"));
    if (QFileInfo(file.fileName()).isSymLink())
        return fail(error, "Project manifest must not be a symbolic link.");
    if (file.size() > 4 * 1024 * 1024 || !file.open(QIODevice::ReadOnly))
        return fail(error, "Cannot read project manifest, or it exceeds 4 MiB.");
    QJsonParseError parse;
    QJsonDocument json = QJsonDocument::fromJson(file.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError || !json.isObject())
        return fail(error, "Invalid project JSON: " + parse.errorString());
    QJsonObject obj = json.object();
    bool comp = obj["format"].toString() == "com.compositor.project";
    if (!comp && obj["format"].toString() != Format)
        return fail(error, "Unrecognized project format.");
    int version = obj["version"].toInt(-1);
    if ((comp && (version < 1 || version > 11)) || (!comp && version != 1 && version != 2))
        return fail(error, "Unsupported project version.");
    if (obj["colorSpace"].toString() != "sRGB")
        return fail(error, "Only sRGB projects are supported.");
    if (comp && !obj["guides"].toArray().isEmpty())
        return fail(error, "This Compositor project has guides, which this version cannot preserve.");
    for (const QString &key : {QString("version"), QString("width"), QString("height")})
        if (!obj[key].isDouble() || obj[key].toDouble() != obj[key].toInt())
            return fail(error, "Project dimensions and version must be integers.");
    if (obj.contains("resolution") && !obj["resolution"].isDouble())
        return fail(error, "Invalid project resolution.");
    State s;
    s.resolution = obj["resolution"].toDouble(72);
    s.size = QSize(obj["width"].toInt(), obj["height"].toInt());
    s.id = obj["documentID"].toString();
    s.active = obj["activeLayerID"].toString();
    if (!Document::validSize(s.size) || !obj["layers"].isArray() ||
        obj["layers"].toArray().size() > MaxLayers)
        return fail(error, "Invalid canvas or layer count.");
    qint64 images = 0, masks = 0;
    for (const auto &value : obj["layers"].toArray()) {
        if (!value.isObject())
            return fail(error, "Layer record must be an object.");
        auto record = value.toObject();
        for (const QString &key : {QString("id"), QString("name")})
            if (!record[key].isString())
                return fail(error, "Layer ID and name must be strings.");
        for (const QString &key :
             {QString("isVisible"), QString("isGroup"), QString("maskEnabled"), QString("maskLinked")})
            if (record.contains(key) && !record[key].isNull() && !record[key].isBool())
                return fail(error, "Invalid boolean layer property: " + key);
        for (const QString &key : {QString("parentID"), QString("imageFile"), QString("maskFile"),
                                   QString("blendMode"), QString("font"), QString("color"), QString("kind"),
                                   QString("textContent"), QString("shapeKind")})
            if (record.contains(key) && !record[key].isNull() && !record[key].isString())
                return fail(error, "Invalid string layer property: " + key);
        if (record.contains("opacity") && !record["opacity"].isDouble())
            return fail(error, "Layer opacity must be a number.");
        Layer l;
        l.id = record["id"].toString();
        l.name = record["name"].toString();
        l.parent = record["parentID"].toString();
        l.visible = record["isVisible"].toBool(true);
        l.opacity = record["opacity"].toDouble(1);
        l.blend = record["blendMode"].toString("Normal");
        l.maskEnabled = record["maskEnabled"].toBool(true);
        l.kind = comp ? (record["isGroup"].toBool() ? "group" : "raster") : record["kind"].toString();
        if (comp) {
            for (const QString &key : {QString("adjustment"), QString("effects"), QString("text"),
                                       QString("shape"), QString("maskSourceID"), QString("maskPlacement")})
                if (record.contains(key) && !record[key].isNull())
                    return fail(error, "Unsupported Compositor feature '" + key + "' in layer '" + l.name +
                                           "'. Export a flattened PNG from Compositor or remove that feature "
                                           "before importing.");
            if (record["maskLinked"].toBool(true) == false)
                return fail(error, "Unlinked Compositor masks are not supported.");
        }
        auto transform = record["transform"].toObject();
        auto origin = transform["origin"].toArray();
        auto size = transform["size"].toArray();
        if (origin.size() != 2 || size.size() != 2 || !origin[0].isDouble() || !origin[1].isDouble() ||
            !size[0].isDouble() || !size[1].isDouble())
            return fail(error, "Invalid layer transform.");
        l.position = QPointF(origin[0].toDouble(), origin[1].toDouble());
        l.size = QSizeF(size[0].toDouble(), size[1].toDouble());
        if (transform.contains("rotation") && !transform["rotation"].isDouble())
            return fail(error, "Layer rotation must be a number.");
        for (const QString &key : {QString("flipX"), QString("flipY")})
            if (transform.contains(key) && !transform[key].isBool())
                return fail(error, "Layer flip must be a boolean.");
        l.rotation = transform["rotation"].toDouble();
        l.flipX = transform["flipX"].toBool();
        l.flipY = transform["flipY"].toBool();
        l.nearest = transform["sampling"].toString() == "Nearest";
        QString image = record["imageFile"].toString(), mask = record["maskFile"].toString();
        if (!image.isEmpty() && !asset(root, image, l.image, images, error))
            return false;
        if (!mask.isEmpty() && !asset(root, mask, l.mask, masks, error))
            return false;
        if (!l.mask.isNull() && !l.image.isNull() && l.mask.size() != l.image.size()) {
            if (comp && l.mask.size() == QSize(1, 1))
                l.mask = l.mask.scaled(l.image.size());
            else
                return fail(error, "Mask dimensions do not match the layer.");
        }
        if (!comp && l.kind == "raster" && image.isEmpty())
            return fail(error, "Raster layer image reference is missing.");
        if (l.kind == "raster" && l.image.isNull()) {
            if (!Document::validSize(l.size.toSize(), MaxSourcePixels) ||
                images + qint64(qRound(l.size.width())) * qRound(l.size.height()) > MaxSourcePixels)
                return fail(error, "Blank layer is too large.");
            l.image = QImage(l.size.toSize(), QImage::Format_ARGB32_Premultiplied);
            l.image.fill(Qt::transparent);
            images += qint64(l.image.width()) * l.image.height();
        }
        if (record.contains("clipped") && !record["clipped"].isBool())
            return fail(error, "Invalid clipping flag.");
        l.clipped = record["clipped"].toBool();
        if (l.kind == "adjustment") {
            auto a = record["adjustment"].toObject();
            if (!a["kind"].isString() || !a["first"].isDouble() || !a["second"].isDouble() ||
                !a["third"].isDouble() || !a["curve"].isArray())
                return fail(error, "Invalid adjustment parameters.");
            l.adjustment = a["kind"].toString();
            l.first = a["first"].toDouble();
            l.second = a["second"].toDouble();
            l.third = a["third"].toDouble();
            for (const auto &v : a["curve"].toArray()) {
                auto point = v.toArray();
                if (point.size() != 2 || !point[0].isDouble() || !point[1].isDouble())
                    return fail(error, "Invalid curve point.");
                l.curve << QPointF(point[0].toDouble(), point[1].toDouble());
            }
        }
        l.text = record["textContent"].toString();
        if (!l.font.fromString(record["font"].toString(l.font.toString())))
            return fail(error, "Invalid font description.");
        l.color = QColor(record["color"].toString("#ff000000"));
        l.shape = record["shapeKind"].toString("rectangle");
        s.layers.append(l);
    }
    if (!validate(s, error))
        return false;
    result = s;
    return true;
}
static QJsonObject manifest(const State &state) {
    QJsonArray layers;
    for (const auto &l : state.layers) {
        QJsonObject record{{"id", l.id},
                           {"name", l.name},
                           {"kind", l.kind},
                           {"parentID", l.parent},
                           {"isVisible", l.visible},
                           {"opacity", l.opacity},
                           {"blendMode", l.blend},
                           {"maskEnabled", l.maskEnabled},
                           {"transform", QJsonObject{{"origin", QJsonArray{l.position.x(), l.position.y()}},
                                                     {"size", QJsonArray{l.size.width(), l.size.height()}},
                                                     {"rotation", l.rotation},
                                                     {"flipX", l.flipX},
                                                     {"flipY", l.flipY},
                                                     {"sampling", l.nearest ? "Nearest" : "Smooth"}}},
                           {"textContent", l.text},
                           {"font", l.font.toString()},
                           {"color", l.color.name(QColor::HexArgb)},
                           {"shapeKind", l.shape}};
        record["clipped"] = l.clipped;
        if (l.kind == "adjustment") {
            QJsonArray points;
            for (const auto &p : l.curve)
                points.append(QJsonArray{p.x(), p.y()});
            record["adjustment"] = QJsonObject{{"kind", l.adjustment},
                                               {"first", l.first},
                                               {"second", l.second},
                                               {"third", l.third},
                                               {"curve", points}};
        }
        layers.append(record);
    }
    return {{"format", Format},
            {"version", 2},
            {"colorSpace", "sRGB"},
            {"documentID", state.id},
            {"width", state.size.width()},
            {"height", state.size.height()},
            {"activeLayerID", state.active},
            {"resolution", state.resolution},
            {"layers", layers}};
}
bool saveProject(const QString &path, const State &state, QString &error) {
    if (!validate(state, error))
        return false;
    QFileInfo target(path);
    if (target.isSymLink() || target.isFile())
        return fail(error, "Project path must be a regular directory.");
    QDir root(target.absoluteFilePath());
    if (!root.mkpath("."))
        return fail(error, "Cannot create project directory.");
    QLockFile lock(root.filePath(".save.lock"));
    if (!lock.tryLock(0))
        return fail(error, "This project is currently being saved by another process.");
    if (QFileInfo(root.filePath("manifest.json")).isSymLink())
        return fail(error, "Project manifest must not be a symbolic link.");
    QJsonObject old;
    if (QFileInfo(root.filePath("manifest.json")).exists()) {
        QFile file(root.filePath("manifest.json"));
        if (!file.open(QIODevice::ReadOnly) || file.size() > 4 * 1024 * 1024)
            return fail(error, "Existing directory is not a PhotoShip project.");
        old = QJsonDocument::fromJson(file.readAll()).object();
        if (old["format"].toString() != Format || old["documentID"].toString() != state.id)
            return fail(error, "Refusing to replace another project. Choose a new project directory.");
    } else {
        QStringList entries = root.entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
        entries.removeAll(".save.lock");
        if (!entries.isEmpty())
            return fail(error, "Existing directory is not an empty directory or a PhotoShip project.");
    }
    if (QFileInfo(root.filePath("images")).isSymLink() || !root.mkpath("images"))
        return fail(error, "Cannot create a safe project asset directory.");
    QJsonObject data = manifest(state);
    auto layers = data["layers"].toArray();
    QStringList written;
    auto abort = [&](const QString &message) {
        for (const auto &name : written)
            QFile::remove(root.filePath("images/" + name));
        if (old.isEmpty())
            root.rmdir("images");
        return fail(error, message);
    };
    for (int i = 0; i < state.layers.size(); ++i) {
        const auto &l = state.layers[i];
        auto record = layers[i].toObject();
        for (bool mask : {false, true}) {
            QImage image = mask ? l.mask : l.image;
            if (image.isNull())
                continue;
            if (mask)
                image = image.convertToFormat(QImage::Format_Grayscale8);
            QString name = l.id + "." + QUuid::createUuid().toString(QUuid::WithoutBraces) +
                           (mask ? ".mask.png" : ".png");
            QString dest = root.filePath("images/" + name);
            written << name;
            if (!image.save(dest, "PNG") || QFileInfo(dest).size() > MaxEncodedBytes)
                return abort("Cannot write project image, or encoded image exceeds 64 MiB. Previous manifest "
                             "was preserved.");
            record[mask ? "maskFile" : "imageFile"] = name;
        }
        layers[i] = record;
    }
    data["layers"] = layers;
    QByteArray json = QJsonDocument(data).toJson();
    if (json.size() > 4 * 1024 * 1024)
        return abort("Project manifest exceeds 4 MiB.");
    QSaveFile file(root.filePath("manifest.json"));
    if (!file.open(QIODevice::WriteOnly) || file.write(json) != json.size() || !file.commit())
        return abort("Cannot commit project manifest. Previous manifest was preserved.");
    // Assets are immutable until the manifest commit. A failed save cannot corrupt existing pixels.
    static QRegularExpression owned("^[A-Fa-f0-9-]{36}\\.[A-Fa-f0-9-]{36}(\\.mask)?\\.png$");
    for (const auto &value : old["layers"].toArray())
        for (const QString &key : {QString("imageFile"), QString("maskFile")}) {
            QString name = value.toObject()[key].toString();
            if (owned.match(name).hasMatch() && !QFileInfo(root.filePath("images/" + name)).isSymLink())
                QFile::remove(root.filePath("images/" + name));
        }
    return true;
}
bool exportImage(const QString &path, const QImage &image, int quality, QString &error) {
    QString suffix = QFileInfo(path).suffix().toLower();
    QByteArray format = suffix == "jpg" || suffix == "jpeg" ? "JPEG" : suffix == "png" ? "PNG" : "";
    if (format.isEmpty())
        return fail(error, "Choose a .png or .jpg output filename.");
    QImage output = image;
    if (format == "JPEG") {
        output = QImage(image.size(), QImage::Format_RGB32);
        output.fill(Qt::white);
        QPainter p(&output);
        p.drawImage(0, 0, image);
        p.end();
        output.setColorSpace(QColorSpace::SRgb);
        output.setDotsPerMeterX(image.dotsPerMeterX());
        output.setDotsPerMeterY(image.dotsPerMeterY());
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return fail(error, "Cannot open export destination.");
    QImageWriter writer(&file, format);
    writer.setQuality(qBound(1, quality, 100));
    if (!writer.write(output))
        return fail(error, "Cannot encode export: " + writer.errorString());
    if (!file.commit())
        return fail(error, "Cannot commit exported image.");
    return true;
}
} // namespace ps
