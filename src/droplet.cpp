/*
    droplet.cpp - Implements the Droplet class

    Copyright (C) 2021 Stewart Reive

    neo is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    neo is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with neo. If not, see <http://www.gnu.org/licenses/>.
*/

#include "droplet.h"
#include "cloud.h"

Droplet::Droplet() {
    Reset();
}

Droplet::Droplet(Cloud* cl, uint16_t col, uint16_t endLine, uint16_t cpIdx,
                 uint16_t len, float cps, milliseconds ttl) {
    Reset();
    _pCloud = cl;
    _boundCol = col;
    _endLine = endLine;
    _charPoolIdx = cpIdx;
    _length = len;
    _charsPerSec = cps;
    _timeToLinger = ttl;
}

void Droplet::Reset() {
    _pCloud = nullptr;
    _isAlive = false;
    _isHeadCrawling = false;
    _isTailCrawling = false;
    _boundCol = 0xFFFF;
    _headPutLine = 0;
    _headCurLine = 0;
    _tailPutLine = 0xFFFF;
    _tailCurLine = 0;
    _endLine = 0xFFFF;
    _charPoolIdx = 0xFFFF;
    _length = 0xFFFF;
    _charsPerSec = 0.0f;
    _lastTime = high_resolution_clock::time_point();
    _headStopTime = high_resolution_clock::time_point();
    _timeToLinger = milliseconds(0);
}

void Droplet::Activate() {
    _isAlive = true;
    _isHeadCrawling = true;
    _isTailCrawling = true;
    _lastTime = high_resolution_clock::now();
}

void Droplet::Advance(high_resolution_clock::time_point curTime) {
    uint64_t elapsedNs = duration_cast<nanoseconds>(curTime - _lastTime).count();
    float elapsedSec = elapsedNs / 1e9;
    uint16_t charsAdvanced = static_cast<uint16_t>(round(_charsPerSec * elapsedSec));
    if (!charsAdvanced)
        return;

    // Advance the head
    if (_isHeadCrawling) {
        _headPutLine += charsAdvanced;
        _headPutLine = min(_headPutLine, _endLine);

        // If head reaches the _endLine, stop the head and maybe the tail too
        if (_headPutLine == _endLine) {
            _isHeadCrawling = false;
            if (!duration_cast<milliseconds>(_headStopTime.time_since_epoch()).count()) {
                _headStopTime = curTime;
                if (_timeToLinger > milliseconds(0)) {
                    _isTailCrawling = false;
                }
            }
        }
    }

    // Advance the tail
    if (_isTailCrawling && (_headPutLine >= _length || _headPutLine >= _endLine)) {
        if (_tailPutLine != 0xFFFF) {
            _tailPutLine += charsAdvanced;
        } else {
            _tailPutLine = charsAdvanced;
        }
        _tailPutLine = min(_tailPutLine, _endLine);

        // If the tail advances far enough down the screen, allow other droplets to spawn
        const uint16_t threshLine = _pCloud->GetLines() / 4;
        if (_tailCurLine <= threshLine && _tailPutLine > threshLine)
            _pCloud->SetColumnSpawn(_boundCol, true);
    }

    // Restart the tail after lingering
    if (!_isTailCrawling && duration_cast<milliseconds>(curTime - _headStopTime) >= _timeToLinger) {
        _isTailCrawling = true;
    }
    // Once tail reaches the head, kill this droplet
    if (_tailPutLine == _headPutLine) {
        _isAlive = false;
    }
    _lastTime = curTime; // Required or else nothing will ever get drawn...
}

void Droplet::Draw(high_resolution_clock::time_point curTime, bool drawEverything) {
    uint16_t startLine = 0;
    if (_tailPutLine != 0xFFFF) {
        // Delete the very end of tail
        for (uint16_t line = _tailCurLine; line <= _tailPutLine; line++) {
            mvaddch(line, _boundCol, ' ');
        }
        _tailCurLine = _tailPutLine;
        startLine = _tailPutLine + 1;
    }
    for (uint16_t line = startLine; line <= _headPutLine; line++) {
        const bool isGlitched = _pCloud->IsGlitched(line, _boundCol);
        const wchar_t val = _pCloud->GetChar(line, _charPoolIdx);

        CharLoc cl = CharLoc::MIDDLE;
        if (_tailPutLine != 0xFFFF && line == _tailPutLine + 1)
            cl = CharLoc::TAIL;
        if (line == _headPutLine && IsHeadBright(curTime))
            cl = CharLoc::HEAD;

        // No need to draw non-glitched chars between tail and _headCurLine
        if (cl == CharLoc::MIDDLE && line < _headCurLine && !isGlitched && line != _endLine &&
            _pCloud->GetShadingMode() != Cloud::ShadingMode::DISTANCE_FROM_HEAD && !drawEverything)
            continue;

        Cloud::CharAttr attr;
        _pCloud->GetAttr(line, _boundCol, val, cl, &attr, curTime, _headPutLine, _length);

        attr_t attr2 = attr.isBold ? A_BOLD : A_NORMAL;
        cchar_t wc = { attr2 };
        wc.chars[0] = val;

        if (_pCloud->GetColorMode() != ColorMode::MONO) {
            attron(COLOR_PAIR(attr.colorPair));
            mvadd_wch(line, _boundCol, &wc);
            attroff(COLOR_PAIR(attr.colorPair));
        } else {
            mvadd_wch(line, _boundCol, &wc);
        }
    }
    _headCurLine = _headPutLine;
}

void Droplet::IncrementTime(milliseconds time) {
    _lastTime += time;
    if (duration_cast<milliseconds>(_headStopTime.time_since_epoch()).count())
        _headStopTime += time;
}

bool Droplet::IsHeadBright(high_resolution_clock::time_point curTime) const {
    if (_isHeadCrawling)
        return true;
    else if (duration_cast<milliseconds>(curTime - _headStopTime) <= milliseconds(100))
        return true;

    return false;
}


