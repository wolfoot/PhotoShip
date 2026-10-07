#pragma once
#include "document.h"
namespace ps {
// PSD v1, 8-bit RGB. Lossy/unsupported features are listed before GUI import.
bool readPsd(const QString &path, State &result, QStringList &report, QString &error);
} // namespace ps
