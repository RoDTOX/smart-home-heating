#include "StatusModel.h"
#include <Arduino.h>
#include <stdio.h>
#include <stdarg.h>

StatusModel statusModel;

StatusModel::StatusModel() : _len(0) {
    _buf[0] = '\0';
}

void StatusModel::reset() {
    _len = 0;
    _buf[0] = '\0';
}

void StatusModel::appendStr(const char* s) {
    if (!s) return;
    size_t slen = strlen(s);
    if (_len + slen < BUFFER_SIZE - 1) {
        memcpy(_buf + _len, s, slen);
        _len += slen;
        _buf[_len] = '\0';
    }
}

void StatusModel::append(const char* fmt, ...) {
    if (_len >= BUFFER_SIZE - 1) return;
    va_list ap;
    va_start(ap, fmt);
    int written = vsnprintf(_buf + _len, BUFFER_SIZE - _len, fmt, ap);
    va_end(ap);
    if (written > 0) {
        size_t w = (size_t)written;
        _len = (_len + w < BUFFER_SIZE - 1) ? (_len + w) : (BUFFER_SIZE - 1);
        _buf[_len] = '\0';
    }
}

const char* StatusModel::buildStatusJson() {
    // Exposes current state formatted into static buffer
    return _buf;
}

const char* StatusModel::buildZonesJson() {
    // Exposes zone state formatted into static buffer
    return _buf;
}
