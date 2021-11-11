/*
    droplet.h

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

#pragma once

#ifndef DROPLET_H
#define DROPLET_H

#include <chrono>
#include <cstdint>

using namespace std::chrono;

class Cloud;

// A Droplet is a single vertical character string
class Droplet {
public:
    Droplet();
    Droplet(Cloud* cl, uint16_t col, uint16_t endLine, uint16_t cpIdx,
            uint16_t len, float cps, milliseconds ttl);

    void Reset();
    void Activate();
    void Advance(high_resolution_clock::time_point curTime);
    void Draw(high_resolution_clock::time_point curTime, bool drawEverything);

    // Getters/Setters/Convenience
    bool IsAlive() const { return _isAlive; }
    uint16_t GetCol() const { return _boundCol; }
    void SetCharsPerSec(float cps) { _charsPerSec = cps; }
    uint16_t GetHeadPutLine() const { return _headPutLine; }
    uint16_t GetTailPutLine() const { return _tailPutLine; }
    uint16_t GetCharPoolIdx() const { return _charPoolIdx; }
    void IncrementTime(milliseconds time); // To facilitate pausing

    enum class CharLoc { // describes where a char is within a Droplet
        MIDDLE,
        TAIL,
        HEAD
    };

private:
    Cloud* _pCloud; // Cloud keeps track of attributes/characters
    bool _isAlive; // Is this Droplet still displaying something?
    bool _isHeadCrawling; // Is the head (bottom) still moving?
    bool _isTailCrawling; // Is the tail (top) still moving?
    uint16_t _boundCol; // Which screen column this droplet renders to
    uint16_t _headPutLine; // Where we are advancing the head
    uint16_t _headCurLine; // Where the head currently is
    uint16_t _tailPutLine; // Where we are advancing the tail
    uint16_t _tailCurLine; // The last empty line in this column
    uint16_t _endLine; // The head will not advance past this line
    uint16_t _charPoolIdx; // Index into the "charPool"
    uint16_t _length; // How many chars is this droplet?
    float _charsPerSec; // How many chars will be drawn per second
    high_resolution_clock::time_point _lastTime; // Last time we drew something
    high_resolution_clock::time_point _headStopTime; // Time when head stopped
    milliseconds _timeToLinger; // How long the droplet is stationary before destruction

    bool IsHeadBright(high_resolution_clock::time_point curTime) const;
};

#endif
