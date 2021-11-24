/*
    cloud.cpp - Implements the Cloud class

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

#include "cloud.h"

#include <cassert>
#include <cstring>

Charset operator&(Charset lhs, Charset rhs) {
    return static_cast<Charset>(
        static_cast<unsigned>(lhs) &
        static_cast<unsigned>(rhs));
}

bool operator!(Charset input) {
    return static_cast<unsigned>(input) == 0;
}

Cloud::Cloud(ColorMode cm, bool def2ascii) :
    _lines(static_cast<uint16_t>(LINES)),
    _cols(COLS),
    _defaultToAscii(def2ascii),
    _colorMode(cm)
{
    assert(stdscr != nullptr);
    if (cm != ColorMode::MONO)
        SetColor(Color::GREEN);
}

void Cloud::Rain() {
    if (_pause)
        return;

    high_resolution_clock::time_point curTime = high_resolution_clock::now();
    SpawnDroplets(curTime);

    if (_forceDrawEverything)
        clear();

    const bool timeForGlitch = TimeForGlitch(curTime);
    for (auto& droplet : _droplets) {
        if (!droplet.IsAlive())
            continue;
        droplet.Advance(curTime);
        if (timeForGlitch)
            DoGlitch(droplet);
        droplet.Draw(curTime, _forceDrawEverything);
        if (!droplet.IsAlive()) {
            auto& cs = _colStat[droplet.GetCol()];
            cs.numDroplets--;

            // If the droplet dies very early, then mark the column as free
            if (droplet.GetTailPutLine() <= _lines / 4)
                cs.canSpawn = true;
        }
    }

    if (!_message.empty()) {
        CalcMessage();
        DrawMessage();
    }

    // Bookkeeping logic for glitching and drawing
    if (timeForGlitch) {
        _lastGlitchTime = curTime;
        _nextGlitchTime = _lastGlitchTime + milliseconds(_randGlitchMs(mt));
    }
    _forceDrawEverything = false;
}

void Cloud::Reset() {
    _lines = static_cast<uint16_t>(LINES);
    _cols = static_cast<uint16_t>(COLS);

    _numDroplets = round(1.5f * _cols);
    _droplets.clear();
    _droplets.resize(_numDroplets);
    for (auto& droplet : _droplets)
        droplet.Reset();

    // Reset all the RNG stuff
    mt.seed(0x1234567);

    int8_t lowPair, highPair;
    if (_numColorPairs < 3) {
        lowPair = 1;
        highPair = 1;
    } else if (_numColorPairs == 3) {
        lowPair = 2;
        highPair = 2;
    } else {
        lowPair = 2;
        highPair = _numColorPairs - 2;
    }
    _randColorPair = uniform_int_distribution<int>(lowPair, highPair);

    _randChance = uniform_real_distribution<float>(0.0f, 1.0f);
    _randLine = uniform_int_distribution<uint16_t>(0, _lines - 2);
    _randCpIdx = uniform_int_distribution<uint16_t>(0, static_cast<uint16_t>(CHAR_POOL_SIZE-1));
    _randLen = uniform_int_distribution<uint16_t>(1, _lines - 2);
    _randCol = uniform_int_distribution<uint16_t>(0, _cols - 1);
    _randGlitchMs = uniform_int_distribution<uint16_t>(_glitchLowMs, _glitchHighMs);
    _randLingerMs = uniform_int_distribution<uint16_t>(_lingerLowMs, _lingerHighMs); // Cannot be 0
    _randSpeed = uniform_real_distribution<float>(0.3333333f, 1.0f);

    const size_t screenSize = _lines * _cols;
    FillGlitchMap(screenSize);
    FillColorMap(screenSize);

    const float dropletSeconds = _lines / _charsPerSec;
    _dropletsPerSec = _cols * _dropletDensity / dropletSeconds;

    _colStat.clear();
    _colStat.resize(_cols);
    for (auto& colStat : _colStat) {
        colStat.maxSpeedPct = 1.0f;
        colStat.numDroplets = 0;
        colStat.canSpawn = true;
    }
    SetAsync(_async);
    SetColumnSpeeds();
    UpdateDropletSpeeds();

    if (!_message.empty())
        ResetMessage();

    _lastGlitchTime = high_resolution_clock::now();
    _nextGlitchTime = _lastGlitchTime + milliseconds(_randGlitchMs(mt));
    _lastSpawnTime = _lastGlitchTime;
}

void Cloud::InitChars() {
    _charPool.resize(CHAR_POOL_SIZE);
    _glitchPool.resize(GLITCH_POOL_SIZE);
    _glitchPoolIdx = 0;
    _chars.clear();
    struct UnicodeRange {
        Charset charset;
        vector<pair<wchar_t, wchar_t>> segments;
    };
    if (_charset == Charset::NONE && _userChars.empty()) {
        _charset = _defaultToAscii ? Charset::DEFAULT : Charset::EXTENDED_DEFAULT;
    }
    vector<UnicodeRange> unicodeRanges = {
        { Charset::BINARY, {{48, 49}} },
        { Charset::HEX, {{48, 57}, {65, 70}} },
        { Charset::ENGLISH_LETTERS, {{65, 90}, {97, 122}} },
        { Charset::ENGLISH_DIGITS, {{48, 57}} },
        { Charset::ENGLISH_PUNCTUATION, {{33, 47}, {58, 64}, {91, 96}, {123, 126}} },
        { Charset::KATAKANA, {{L'\uFF64', L'\uFF9F'}} },
        { Charset::GREEK, {{L'\u0370', L'\u03FF'}} },
        { Charset::CYRILLIC, {{L'\u0410', L'\u044F'}} },
        { Charset::ARABIC, {{L'\u0627', L'\u0649'}} },
        { Charset::HEBREW, {{L'\u0590', L'\u05FF'}, {L'\uFB1D', L'\uFB4F'}} },
        { Charset::DEVANAGARI, {{L'\u0900', L'\u097F'}} },
    };
    size_t numRanges = unicodeRanges.size();
    for (size_t range = 0; range < numRanges; range++) {
        UnicodeRange& theRange = unicodeRanges[range];
        if (!(_charset & theRange.charset))
            continue;
        for (const auto& segment : theRange.segments)
            for (wchar_t wchar = segment.first; wchar <= segment.second; wchar++)
                _chars.push_back(wchar);
    }
    _chars.insert(_chars.end(), _userChars.begin(), _userChars.end());
    _randCharIdx = uniform_int_distribution<size_t>(0, _chars.size()-1);
    for (size_t ii = 0; ii < CHAR_POOL_SIZE; ii++)
        _charPool[ii] = _chars[_randCharIdx(mt)];
    for (size_t ii = 0; ii < GLITCH_POOL_SIZE; ii++)
        _glitchPool[ii] = _chars[_randCharIdx(mt)];
}

void Cloud::FillDroplet(Droplet* pDroplet, uint16_t col) {
    uint16_t endLine = _lines - 1;
    if (_randChance(mt) <= _dieEarlyPct)
        endLine = _randLine(mt);
    uint16_t cpIdx = _randCpIdx(mt);
    uint16_t len = _lines;
    if (_randChance(mt) <= _shortPct)
        len = _randLen(mt);
    milliseconds ttl = milliseconds(1);
    if (endLine <= len)
        ttl = milliseconds(_randLingerMs(mt));
    const float speed = _colStat[col].maxSpeedPct * _charsPerSec;
    *pDroplet = Droplet(this, col, endLine, cpIdx, len, speed, ttl);
}

bool Cloud::TimeForGlitch(high_resolution_clock::time_point time) const {
    return _glitchy ? (time >= _nextGlitchTime) : false;
}

void Cloud::DoGlitch(const Droplet& droplet) {
    if (!_glitchy)
        return;
    uint16_t startLine = 0;
    const uint16_t tpLine = droplet.GetTailPutLine();
    if (tpLine != 0xFFFF)
        startLine = tpLine + 1;

    const uint16_t hpLine = droplet.GetHeadPutLine();
    const uint16_t col = droplet.GetCol();
    const uint16_t cpIdx = droplet.GetCharPoolIdx();

    for (uint16_t line = startLine; line <= hpLine; line++) {
        if (IsGlitched(line, col)) {
            const size_t charIdx = (cpIdx + line) % Cloud::CHAR_POOL_SIZE;
            assert(charIdx < _charPool.size());
            assert(_glitchPoolIdx < _glitchPool.size());
            _charPool[charIdx] = _glitchPool[_glitchPoolIdx];
            _glitchPoolIdx = (_glitchPoolIdx + 1) % GLITCH_POOL_SIZE;
        }
    }
}

bool Cloud::IsBright(high_resolution_clock::time_point time) const {
    if (time < _lastGlitchTime)
        return false;
    uint64_t timeSinceGlitch = duration_cast<nanoseconds>(time - _lastGlitchTime).count();
    uint64_t timeBetweenGlitches = duration_cast<nanoseconds>(_nextGlitchTime - _lastGlitchTime).count();
    return static_cast<double>(timeSinceGlitch) / timeBetweenGlitches <= 0.25;
}

bool Cloud::IsDim(high_resolution_clock::time_point time) const {
    if (time > _nextGlitchTime)
        return true;
    uint64_t timeSinceGlitch = duration_cast<nanoseconds>(time - _lastGlitchTime).count();
    uint64_t timeBetweenGlitches = duration_cast<nanoseconds>(_nextGlitchTime - _lastGlitchTime).count();
    return static_cast<double>(timeSinceGlitch) / timeBetweenGlitches >= 0.75;
}

void Cloud::GetAttr(uint16_t line, uint16_t col, wchar_t val, Droplet::CharLoc ct,
                    CharAttr* pAttr, high_resolution_clock::time_point time,
                    uint16_t headPutLine, uint16_t length) const {
    if (_boldMode == BoldMode::RANDOM)
        pAttr->isBold = ((line ^ val) % 2 == 1);
    const size_t idx = col * _lines + line;
    pAttr->colorPair = _colorPairMap.at(idx);
    if (_shadingMode == ShadingMode::DISTANCE_FROM_HEAD) {
        pAttr->colorPair = _numColorPairs -
            round(static_cast<float>(headPutLine - line) / length *
                  static_cast<float>(_numColorPairs - 1));
    }
    if (_glitchy && _glitchMap.at(idx)) {
        if (IsBright(time)) {
            pAttr->colorPair++;
            pAttr->isBold = true;
        } else if (IsDim(time)) {
            pAttr->colorPair--;
            pAttr->isBold = false;
        }
    }
    switch (ct) {
        case Droplet::CharLoc::TAIL:
            pAttr->colorPair = 1;
            pAttr->isBold = false;
            break;
        case Droplet::CharLoc::HEAD:
            pAttr->colorPair = _numColorPairs;
            pAttr->isBold = true;
            break;
        case Droplet::CharLoc::MIDDLE: // fallthrough
        default:
            pAttr->colorPair = min(pAttr->colorPair, _numColorPairs - 1);
            pAttr->colorPair = max(pAttr->colorPair, 1);
            break;
    }
    if (_boldMode == BoldMode::OFF)
        pAttr->isBold = false;
    else if (_boldMode == BoldMode::ALL)
        pAttr->isBold = true;
}

void Cloud::SetCharsPerSec(float cps) {
    // Values below 0.1 are broken for some reason
    _charsPerSec = max(cps, 0.25f);

    const float dropletSeconds = _lines / _charsPerSec;
    _dropletsPerSec = _cols * _dropletDensity / dropletSeconds;

    SetColumnSpeeds();
    UpdateDropletSpeeds();
}

wchar_t Cloud::GetChar(uint16_t line, uint16_t charPoolIdx) const {
    const size_t charIdx = (charPoolIdx + line) % Cloud::CHAR_POOL_SIZE;
    assert(charIdx < _charPool.size());
    return _charPool[charIdx];
}

bool Cloud::IsGlitched(uint16_t line, uint16_t col) const {
    if (!_glitchy)
        return false;
    const size_t mapIdx = col * _lines + line;
    assert(mapIdx < _glitchMap.size());
    return _glitchMap[mapIdx];
}

void Cloud::TogglePause() {
    _pause = !_pause;
    if (_pause) {
        _pauseTime = high_resolution_clock::now();
    } else {
        auto elapsed = duration_cast<milliseconds>(high_resolution_clock::now() - _pauseTime);
        _lastSpawnTime += elapsed;
        for (auto& droplet : _droplets) {
            if (!droplet.IsAlive())
                continue;
            droplet.IncrementTime(elapsed);
        }
    }
}

void Cloud::SetColor(Color c) {
    _color = c;
    use_default_colors();
    int bgColor = 16;
    if (_colorMode == ColorMode::COLOR16)
        bgColor = 0;
    if (_defaultBackground)
        bgColor = -1;
    switch (_color) {
        case Color::USER: {
            if (_colorMode == ColorMode::TRUECOLOR) {
                for (const auto& colorContent : _usrColors) {
                    if (colorContent.r == 0x7FFF || colorContent.g == 0x7FFF || colorContent.b == 0x7FFF)
                        continue;
                    init_color(colorContent.color, colorContent.r, colorContent.g, colorContent.b);
                }
            }
            bgColor = _usrColors[0].color;
            for (_numColorPairs = 1; static_cast<size_t>(_numColorPairs) < _usrColors.size(); _numColorPairs++)
                init_pair(_numColorPairs, _usrColors[_numColorPairs].color, bgColor);
            _numColorPairs--;
            break;
        }
        case Color::GREEN: {
            if (_colorMode == ColorMode::TRUECOLOR) {
                init_color(234, 71, 141, 83);
                init_color(22, 149, 243, 161);
                init_color(28, 188, 596, 318);
                init_color(35, 188, 714, 397);
                init_color(78, 227, 925, 561);
                init_color(84, 271, 973, 667);
                init_color(159, 667, 1000, 941);
            }
            if (_colorMode == ColorMode::COLOR16) {
                _numColorPairs = 2;
                init_pair(1, 10, bgColor);
                init_pair(2, 15, bgColor);
            } else {
                _numColorPairs = 7;
                init_pair(1, 234, bgColor); // normal-4
                init_pair(2, 22, bgColor);  // normal-3
                init_pair(3, 28, bgColor);  // normal-2
                init_pair(4, 35, bgColor);  // normal-1
                init_pair(5, 78, bgColor);  // normal green
                init_pair(6, 84, bgColor);  // bright green
                init_pair(7, 159, bgColor); // leading edge
            }
            break;
        }
        case Color::GOLD: {
            if (_colorMode == ColorMode::TRUECOLOR) {
                init_color(58, 839, 545, 216);
                init_color(94, 905, 694, 447);
                init_color(172, 945, 831, 635);
                init_color(178, 1000, 922, 565);
                init_color(228, 1000, 953, 796);
                init_color(230, 976, 976, 968);
            }
            if (_colorMode == ColorMode::COLOR16) {
                _numColorPairs = 4;
                init_pair(1, 8, bgColor);
                init_pair(2, 3, bgColor);
                init_pair(3, 11, bgColor);
                init_pair(4, 15, bgColor);
            } else {
                _numColorPairs = 7;
                init_pair(1, 58, bgColor); // rgb=44,23,0
                init_pair(2, 94, bgColor); // rgb=135,78,26
                init_pair(3, 172, bgColor); // rgb=214,139,55
                init_pair(4, 178, bgColor); // rgb=211.137,53
                init_pair(5, 228, bgColor); // rgb=255,235,144
                init_pair(6, 230, bgColor); // rgb=255, 243, 203
                init_pair(7, 231, bgColor); // pure white
            }
            break;
        }
        case Color::GREEN2: {
            if (_colorMode == ColorMode::TRUECOLOR) {
                init_color(28, 16, 180, 59);
                init_color(34, 59, 246, 117);
                init_color(76, 46, 512, 172);
                init_color(84, 262, 749, 332);
                init_color(120, 520, 945, 578);
                init_color(157, 676, 969, 758);
                init_color(231, 906, 1000, 898);
            }
            if (_colorMode == ColorMode::COLOR16) {
                _numColorPairs = 4;
                init_pair(1, 8, bgColor);
                init_pair(2, 2, bgColor);
                init_pair(3, 10, bgColor);
                init_pair(4, 15, bgColor);
            } else {
                _numColorPairs = 7;
                init_pair(1, 28, bgColor);
                init_pair(2, 34, bgColor);
                init_pair(3, 76, bgColor);
                init_pair(4, 84, bgColor);
                init_pair(5, 120, bgColor);
                init_pair(6, 157, bgColor);
                init_pair(7, 231, bgColor);
            }
            break;
        }
        case Color::GREEN3: {
            if (_colorMode == ColorMode::TRUECOLOR) {
                init_color(22, 0, 373, 0);
                init_color(28, 0, 529, 0);
                init_color(34, 0, 686, 0);
                init_color(70, 373, 686, 0);
                init_color(76, 373, 843, 0);
                init_color(82, 373, 1000, 0);
                init_color(157, 686, 1000, 686);
            }
            if (_colorMode == ColorMode::COLOR16) {
                _numColorPairs = 2;
                init_pair(1, 2, bgColor);
                init_pair(2, 15, bgColor);
            } else {
                _numColorPairs = 7;
                init_pair(1, 22, bgColor);
                init_pair(2, 28, bgColor);
                init_pair(3, 34, bgColor);
                init_pair(4, 70, bgColor);
                init_pair(5, 76, bgColor);
                init_pair(6, 82, bgColor);
                init_pair(7, 157, bgColor);
            }
            break;
        }
        case Color::YELLOW: {
            if (_colorMode == ColorMode::COLOR16) {
                _numColorPairs = 3;
                init_pair(1, 8, bgColor);
                init_pair(2, 11, bgColor);
                init_pair(3, 15, bgColor);
            } else {
                _numColorPairs = 7;
                init_pair(1, 100, bgColor);
                init_pair(2, 142, bgColor);
                init_pair(3, 184, bgColor);
                init_pair(4, 226, bgColor);
                init_pair(5, 227, bgColor);
                init_pair(6, 229, bgColor);
                init_pair(7, 230, bgColor);
            }
            break;
        }
        case Color::RAINBOW: {
            if (_colorMode == ColorMode::COLOR16) {
                _numColorPairs = 6;
                init_pair(1, 9, bgColor);
                init_pair(2, 1, bgColor);
                init_pair(3, 11, bgColor);
                init_pair(4, 10, bgColor);
                init_pair(5, 12, bgColor);
                init_pair(6, 13, bgColor);
            } else {
                _numColorPairs = 7;
                init_pair(1, 196, bgColor);
                init_pair(2, 208, bgColor);
                init_pair(3, 226, bgColor);
                init_pair(4, 46, bgColor);
                init_pair(5, 21, bgColor);
                init_pair(6, 93, bgColor);
                init_pair(7, 201, bgColor);
            }
            break;
        }
        case Color::RED: {
            if (_colorMode == ColorMode::COLOR16) {
                _numColorPairs = 3;
                init_pair(1, 1, bgColor);
                init_pair(2, 9, bgColor);
                init_pair(3, 15, bgColor);
            } else {
                _numColorPairs = 7;
                init_pair(1, 234, bgColor);
                init_pair(2, 52, bgColor);
                init_pair(3, 88, bgColor);
                init_pair(4, 124, bgColor);
                init_pair(5, 160, bgColor);
                init_pair(6, 196, bgColor);
                init_pair(7, 217, bgColor);
            }
            break;
        }
        case Color::BLUE: {
            if (_colorMode == ColorMode::COLOR16) {
                _numColorPairs = 3;
                init_pair(1, 4, bgColor);
                init_pair(2, 12, bgColor);
                init_pair(3, 15, bgColor);
            } else {
                _numColorPairs = 7;
                init_pair(1, 234, bgColor);
                init_pair(2, 17, bgColor);
                init_pair(3, 18, bgColor);
                init_pair(4, 19, bgColor);
                init_pair(4, 20, bgColor);
                init_pair(5, 21, bgColor);
                init_pair(6, 75, bgColor);
                init_pair(7, 159, bgColor);
            }
            break;
        }
        case Color::CYAN: {
            if (_colorMode == ColorMode::COLOR16) {
                _numColorPairs = 3;
                init_pair(1, 6, bgColor);
                init_pair(2, 14, bgColor);
                init_pair(3, 15, bgColor);
            } else {
                _numColorPairs = 7;
                init_pair(1, 24, bgColor);
                init_pair(2, 25, bgColor);
                init_pair(3, 31, bgColor);
                init_pair(4, 32, bgColor);
                init_pair(5, 38, bgColor);
                init_pair(6, 45, bgColor);
                init_pair(7, 159, bgColor);
            }
            break;
        }
        case Color::ORANGE:
        {
            if (_colorMode == ColorMode::COLOR16) {
                // Orange isn't really achievable in 16 color mode...
                _numColorPairs = 2;
                init_pair(1, 1, bgColor);
                init_pair(2, 7, bgColor);
            } else {
                _numColorPairs = 7;
                init_pair(1, 52, bgColor);
                init_pair(2, 94, bgColor);
                init_pair(3, 130, bgColor);
                init_pair(4, 166, bgColor);
                init_pair(5, 202, bgColor);
                init_pair(6, 208, bgColor);
                init_pair(7, 231, bgColor);
            }
            break;
        }
        case Color::PURPLE:
        {
            if (_colorMode == ColorMode::COLOR16) {
                _numColorPairs = 2;
                init_pair(1, 5, bgColor);
                init_pair(2, 7, bgColor);
            } else {
                _numColorPairs = 7;
                init_pair(1, 60, bgColor);
                init_pair(2, 61, bgColor);
                init_pair(3, 62, bgColor);
                init_pair(4, 63, bgColor);
                init_pair(5, 69, bgColor);
                init_pair(6, 111, bgColor);
                init_pair(7, 225, bgColor);
            }
            break;
        }
        case Color::PINK:
        {
            if (_colorMode == ColorMode::COLOR16) {
                _numColorPairs = 2;
                init_pair(1, 13, bgColor);
                init_pair(2, 15, bgColor);
            } else {
                _numColorPairs = 7;
                init_pair(1, 133, bgColor);
                init_pair(2, 139, bgColor);
                init_pair(3, 176, bgColor);
                init_pair(4, 212, bgColor);
                init_pair(5, 218, bgColor);
                init_pair(6, 224, bgColor);
                init_pair(7, 231, bgColor);
            }
            break;
        }
        case Color::PINK2:
        {
            if (_colorMode == ColorMode::COLOR16) {
                _numColorPairs = 3;
                init_pair(1, 5, bgColor);
                init_pair(2, 13, bgColor);
                init_pair(3, 15, bgColor);
            } else {
                _numColorPairs = 7;
                init_pair(1, 145, bgColor);
                init_pair(2, 181, bgColor);
                init_pair(3, 217, bgColor);
                init_pair(4, 218, bgColor);
                init_pair(5, 224, bgColor);
                init_pair(6, 225, bgColor);
                init_pair(7, 231, bgColor);
            }
            break;
        }
        case Color::VAPORWAVE:
        {
            if (_colorMode == ColorMode::COLOR16) {
                _numColorPairs = 5;
                init_pair(1, 5, bgColor);
                init_pair(2, 13, bgColor);
                init_pair(3, 11, bgColor);
                init_pair(4, 14, bgColor);
                init_pair(5, 15, bgColor);
            } else {
                _numColorPairs = 15;
                init_pair(1, 53, bgColor); // dark purple
                init_pair(2, 54, bgColor);
                init_pair(3, 55, bgColor);
                init_pair(4, 134, bgColor); // light purple/pink
                init_pair(5, 177, bgColor);
                init_pair(6, 219, bgColor);
                init_pair(7, 214, bgColor); // Orange/yellow
                init_pair(8, 220, bgColor);
                init_pair(9, 227, bgColor);
                init_pair(10, 229, bgColor);
                init_pair(11, 87, bgColor); // cyan
                init_pair(12, 123, bgColor);
                init_pair(13, 159, bgColor);
                init_pair(14, 195, bgColor);
                init_pair(15, 231, bgColor); // white
            }
            break;
        }
        case Color::GRAY:
        {
            if (_colorMode == ColorMode::COLOR16) {
                _numColorPairs = 3;
                init_pair(1, 8, bgColor);
                init_pair(2, 7, bgColor);
                init_pair(3, 15, bgColor);
            } else {
                _numColorPairs = 9;
                init_pair(1, 234, bgColor);
                init_pair(2, 237, bgColor);
                init_pair(3, 240, bgColor);
                init_pair(4, 243, bgColor);
                init_pair(5, 246, bgColor);
                init_pair(6, 249, bgColor);
                init_pair(7, 251, bgColor);
                init_pair(8, 252, bgColor);
                init_pair(9, 231, bgColor);
            }
            break;
        }
        default:
            break;
    }

    int8_t lowPair, highPair;
    if (_numColorPairs < 3) {
        lowPair = 1;
        highPair = 1;
    } else if (_numColorPairs == 3) {
        lowPair = 2;
        highPair = 2;
    } else {
        lowPair = 2;
        highPair = _numColorPairs - 2;
    }
    _randColorPair.param(std::uniform_int_distribution<int>::param_type{lowPair, highPair});
    _randColorPair.reset();
    const size_t screenSize = _lines * _cols;
    FillColorMap(screenSize);

    if (_colorMode != ColorMode::MONO)
        bkgd(COLOR_PAIR(1));
    ForceDrawEverything();
}

void Cloud::SpawnDroplets(high_resolution_clock::time_point curTime) {
    const nanoseconds elapsed = duration_cast<nanoseconds>(curTime - _lastSpawnTime);
    const float elapsedSec = static_cast<float>(elapsed.count() / 1e9);
    const size_t dropletsToSpawn = min(static_cast<size_t>(elapsedSec * _dropletsPerSec),
                                       _numDroplets);
    if (!dropletsToSpawn)
        return;

    size_t dropletIdx = 0;
    int dropletsSpawned = 0;
    for (size_t ii = 0; ii < dropletsToSpawn; ii++) {
        uint16_t col = _randCol(mt);
        if (_fullWidth)
            col &= 0xFFFE;
        if (!_colStat[col].canSpawn || _colStat[col].numDroplets >= _maxDropletsPerColumn)
            continue;
        Droplet* dropletToSpawn = nullptr;
        for (; dropletIdx < _numDroplets; dropletIdx++) {
            if (!_droplets[dropletIdx].IsAlive()) {
                dropletToSpawn = &_droplets[dropletIdx];
                break;
            }
        }
        if (!dropletToSpawn)
            break;
        FillDroplet(dropletToSpawn, col);
        dropletToSpawn->Activate();
        _colStat[col].canSpawn = false;
        _colStat[col].numDroplets++;
        dropletsSpawned++;
    }
    if (dropletsSpawned)
        _lastSpawnTime = curTime;
}

void Cloud::SetDropletDensity(float density) {
    _dropletDensity = density;
    const float dropletSeconds = _lines / _charsPerSec;
    _dropletsPerSec = _cols * _dropletDensity / dropletSeconds;
}

void Cloud::SetColumnSpeeds() {
    for (auto& col : _colStat)
        col.maxSpeedPct = _async ? _randSpeed(mt) : 1.0f;
}

void Cloud::UpdateDropletSpeeds() {
    for (auto& droplet : _droplets) {
        if (!droplet.IsAlive())
            continue;
        droplet.SetCharsPerSec(_colStat[droplet.GetCol()].maxSpeedPct * _charsPerSec);
    }
}

void Cloud::SetColumnSpawn(uint16_t col, bool b) {
    assert(col < _colStat.size());
    _colStat[col].canSpawn = b;
}

void Cloud::AddChars(wchar_t begin, wchar_t end) {
    if (begin > end)
        Die("--chars: characters given in wrong order\n");

    while (begin <= end) {
        _userChars.push_back(begin++);
    }
}

void Cloud::SetGlitchPct(float pct) {
    _glitchPct = pct;
    FillGlitchMap(_lines * _cols);
}

void Cloud::FillGlitchMap(size_t screenSize) {
    if (!_glitchy)
        return;
    _glitchMap.resize(screenSize);
    for (size_t i = 0; i < screenSize; i++) {
        _glitchMap[i] = _randChance(mt) <= _glitchPct;
    }
}

void Cloud::SetGlitchTimes(uint16_t low_ms, uint16_t high_ms) {
    _glitchLowMs = low_ms;
    _glitchHighMs = high_ms;
}

void Cloud::SetLingerTimes(uint16_t low_ms, uint16_t high_ms) {
    _lingerLowMs = low_ms;
    _lingerHighMs = high_ms;
}

void Cloud::SetMessage(const char* msg) {
    while (*msg)
        _message.emplace_back(*msg++);
}

void Cloud::FillColorMap(size_t screenSize) {
    _colorPairMap.resize(screenSize);
    for (size_t i = 0; i < screenSize; i++) {
        _colorPairMap[i] = _randColorPair(mt);
    }
}

// Reset the position of all message chars and clear them.
// The message is centered between the first and last quarter
// of the screen.
void Cloud::ResetMessage() {
    const uint16_t firstCol = _cols / 4;
    const uint16_t lastCol = 3 * _cols / 4;
    const uint16_t charsPerCol = lastCol - firstCol + 1;
    const uint16_t msgLines = static_cast<uint16_t>(_message.size() / charsPerCol + 1);
    const uint16_t firstLine = _lines / 2 - msgLines / 2;

    size_t charsRemaining = _message.size();
    uint16_t line = firstLine;
    uint16_t col = firstCol;
    if (charsRemaining < charsPerCol)
        col += (charsPerCol - static_cast<uint16_t>(charsRemaining)) / 2;

    for (auto& msgChar : _message) {
        msgChar.draw = false;
        if (line < _lines) {
            msgChar.line = line;
            msgChar.col = col;
        } else {
            msgChar.line = 0xFFFF;
            msgChar.col = 0xFFFF;
        }
        if (col == lastCol) {
            line++;
            col = firstCol;
            if (charsRemaining < charsPerCol) {
                col += (charsPerCol - static_cast<uint16_t>(charsRemaining)) / 2;
            }
        } else {
            col++;
        }
        charsRemaining--;
    }
}

// Find which chars in the message should be drawn
void Cloud::CalcMessage() {
    wchar_t wc[2];
    for (auto& msgChar : _message) {
        if (msgChar.line == 0xFFFF || msgChar.col == 0xFFFF)
            break;

        mvinnwstr(msgChar.line, msgChar.col, wc, 1);
        if (wc[0] != 0 && wc[0] != ' ')
            msgChar.draw = true;
    }
}

void Cloud::DrawMessage() const {
    for (const auto& msgChar : _message) {
        if (!msgChar.draw)
            continue;

        const attr_t attr = (_boldMode == BoldMode::OFF) ? A_NORMAL : A_BOLD;
        cchar_t wc = {};
        wc.attr = attr;
        wc.chars[0] = msgChar.val;
        if (_colorMode != ColorMode::MONO)
            attron(COLOR_PAIR(_numColorPairs));

        mvadd_wch(msgChar.line, msgChar.col, &wc);

        if (_colorMode != ColorMode::MONO)
            attroff(COLOR_PAIR(_numColorPairs));
    }
}
