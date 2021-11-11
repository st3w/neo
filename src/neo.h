/*
    neo.h

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

#ifndef NEO_H
#define NEO_H

#include <climits>

/**************************************
 *               Enums                *
 *************************************/

enum class Charset : unsigned {
    NONE = 0x0,
    ENGLISH_LETTERS = 0x1,
    ENGLISH_DIGITS = 0x2,
    ENGLISH_PUNCTUATION = 0x4,
    KATAKANA = 0x8,
    GREEK = 0x10,
    CYRILLIC = 0x20,
    ARABIC = 0x40,
    HEBREW = 0x80,
    BINARY = 0x100,
    HEX = 0x200,
    DEVANAGARI = 0x400,
    DEFAULT = 0x7,
    EXTENDED_DEFAULT = 0xE,
};

enum class Color : unsigned {
    USER,
    GREEN, // Modeled after the Neo/Cypher scene
    GREEN2, // from the Matrix Reloaded opening crawl
    GREEN3, // salt to taste, yellow-green
    YELLOW,
    ORANGE,
    RED,
    BLUE,
    CYAN,
    GOLD, // Seraph gold
    RAINBOW,
    PURPLE,
    PINK,
    PINK2,
    VAPORWAVE,
    GRAY
};

enum class ColorMode {
    MONO, // no color
    COLOR16, // 16 colors
    COLOR256, // 256 colors
    TRUECOLOR, // 32-bit color
    INVALID
};

/**************************************
 *              Structs               *
 *************************************/

struct ColorContent {
    ColorContent() = default;
    explicit ColorContent(short col) : color(col) {}
    ColorContent(short col, short red, short green, short blue) :
        color(col), r(red), g(green), b(blue) {}

    short color = 0;
    short r = 0x7FFF;
    short g = 0x7FFF;
    short b = 0x7FFF;
};

/**************************************
 *             Functions              *
 *************************************/

void Cleanup();

// Print a message to stderr and exit neo with RETVAL=1
void Die(const char* fmtStr, ...);

#endif
