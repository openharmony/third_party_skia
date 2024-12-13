// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <unicode/utf.h>
#include <unicode/utf8.h>

using namespace std;

// upper limit for direct pointing arrays
#define MAXIMUM_DIRECT_CODE_POINT 0x7a

// print all leafs and patterns (in 16bit hex), lots of information
//#define VERBOSE_PATTERNS

namespace {
// convert utf8 string to utf16
vector<uint16_t> convertToUTF16(const string& target_u8)
{
    vector<uint16_t> target;
    int32_t i = 0;
    const int32_t textLength = target_u8.size();
    uint32_t c = 0;
    while (i < textLength) {
        U8_NEXT(target_u8.c_str(), i, textLength, c);
        if (U16_LENGTH(c) == 1) {
            target.push_back(c);
        } else {
            target.push_back(U16_LEAD(c));
            target.push_back(U16_TRAIL(c));
        }
    }
    return target;
}
}

// Recursive path implementation.
// Collects static information and the leafs that provide access to patterns
// The implementation is reversed to pattern code point order; end to beginning of pattern;
struct Path {
    explicit Path(const vector<uint16_t>& path, const vector<uint8_t>* pat)
    {
        count++;
        size_t ii = path.size();
        if (ii > 0) {
            code = path[--ii];
        }

        // process children recursively
        if (ii > 0) {
            process(path, ii, pat);
        } else {
            // store pattern to leafs
            pattern = pat;
            // this is not very clear division yet, but generally
            // the direct array size needs to be limited
            if (code <= MAXIMUM_DIRECT_CODE_POINT && code != '.' && code != '\'') {
                maximumCP = max(maximumCP, code);
                minimumCP = min(minimumCP, code);
            }
            leafCount++;
        }
    }

    void process(const vector<uint16_t>& path, size_t ii, const vector<uint8_t>* pat)
    {
        if (ii > 0) {
            uint16_t key = path[--ii];
            if (auto ite = paths.find(key); ite != paths.end()) {
                ite->second.process(path, ii, pat);
            } else {
                if (key > MAXIMUM_DIRECT_CODE_POINT || key == '.' || key == '\'') {
                    // if we have direct children with distinct code points, we need to use
                    // value pairs
                    haveNoncontiguousChildren = true;
                }
                vector<uint16_t> substr(path.cbegin(), path.cbegin() + ii + 1);
                // recurse
                paths.emplace(key, Path(substr, pat));
            }
        }
    }

    // Once this node is reached, we can access pattern
    // instead traversing further
    bool isLeaf() const
    {
        return pattern != nullptr;
    }

    // This instance of Path and its children implement a straight path without ambquity
    // No need to traverse through tables to reach pattern.
    // Calculate the depth of the graph.
    bool isLinear(size_t& count) const
    {
        count++;
        if (paths.size() == 0) {
            return true;
        } else if (paths.size() == 1) {
            return paths.begin()->second.isLinear(count);
        }
        return false;
    }

    // debug print misc info
    void print(size_t indent) const
    {
#ifdef VERBOSE_PATTERNS
        indent += 2;
        for (size_t ii = 0; ii < indent; ii++)
            cout << " ";
        if (indent == 12) {
            cout << char(code) << "rootsize***: " << paths.size();
        } else {
            cout << char(code) << "***: " << paths.size();
        }
        size_t dummy { 0 };
        if (paths.size() >= 8)
            cout << " ###";
        else if (isLinear(dummy)) {
            cout << " !!!";
        } else {
            cout << " @@@";
        }

        cout << endl;
        if (paths.size() == 0)
            return;
        for (auto path : paths)
            path.second.print(indent);
        cout << endl;
#endif
    }

    // todo use template
    static void writePacked(vector<uint16_t>& data, ostream& out, bool writeCount = true)
    {
        uint16_t size = data.size();
        if (writeCount) {
            out.write((const char*)&size, sizeof(size));
        }

        if (data.size() & 0x1) {
            data.push_back(0);
        }

        for (size_t ii = 0; ii < data.size() / 2; ii++) {
            uint32_t bytes = data[ii * 2] | data[ii * 2 + 1] << 16;
            out.write((const char*)&bytes, sizeof(bytes));
        }
    }

    static void writePacked(const vector<uint8_t>& data, ostream& out)
    {
        uint16_t size = data.size();
        out.write((const char*)&size, sizeof(size));

        if (data.size() & 0x3) {
            cerr << "### uint8_t vectors should be aligned in 4 bytes !!!" << endl;
        }

        for (size_t ii = 0; ii < size / 4; ii++) {
            uint32_t bytes = data[ii * 4] | data[ii * 4 + 1] << 8 | data[ii * 4 + 2] << 16 | data[ii * 4 + 3] << 24;
            out.write((const char*)&bytes, sizeof(bytes));
        }
    }

    // no need to twiddle the bytes or words currently
    static void writePacked(uint32_t word, ostream& out)
    {
        out.write((const char*)&word, sizeof(word));
    }

    // We make assumption that 14 bytes is enough to represent offset
    // so we get two first bits in the array for path type
    // we have two bytes on the offset arrays
    // for these
    enum class PathType : uint8_t {
        PATTERN = 0,
        LINEAR = 1,
        PAIRS = 2,
        DIRECT = 3
    };

    uint16_t write(ostream& out, uint32_t offset = 0, uint16_t* endPos = 0, bool breakForCheck = false) const
    {
        if (minimumCP > maximumCP) {
            cerr << "Minimum code point cannot be smaller than maximum, bailing out" << endl;
            return 0;
        }

        PathType type { PathType::DIRECT };

        // kind of laymans conditional breakpoint, could be removed completely
        if (breakForCheck) {
            cout << "### raw 16-bit offset: " << out.tellp() / 2 << endl;
        }

        // store the current offset
        uint32_t pos = out.tellp();

        // check if we are linear or should write a table
        size_t count = 0;
        if (isLinear(count)) {
            // this instance is linear, also write pattern
            vector<uint16_t> output;
            auto path = this;
            auto localPattern = pattern;

            if (!path->paths.empty()) {
                auto ite = path->paths.cbegin();
                path = &(ite->second);
                localPattern = path->pattern;
            }
            while (path) {
                output.push_back(path->code);
                if (!path->paths.empty()) {
                    auto ite = path->paths.cbegin();
                    path = &(ite->second);
                    localPattern = path->pattern;
                } else {
                    break;
                }
            }

            // if we have multiple children, they need to be checked
            // when collecting rules
            if (output.size() > 1) {
                type = PathType::LINEAR;
                writePacked(output, out);
            } else {
                // otherwise ce can directly jump to pattern
                type = PathType::PATTERN;
                if (output.empty()) {
                    output.push_back(0);
                }
                out.write((const char*)&output[0], sizeof(output[0]));
            }
            if (localPattern) {
                writePacked(*localPattern, out);
            } else {
                cerr << "Could not resolve pattern on the linear path !!!" << endl;
            }
        } else if ((paths.size() < (size_t)(maximumCP - minimumCP) / 2) || haveNoncontiguousChildren) {
            // Using dense table, i.e. value pairs
            // we need to use this also when the code is larger than ff
            vector<uint16_t> output;
            for (const auto& path : paths) {
                output.push_back(path.first);
                output.push_back(path.second.write(out, offset));
            }
            pos = out.tellp(); // our header is after children data
            type = PathType::PAIRS;
            writePacked(output, out);
        } else {
            // Direct pointing, initialize full mapping table
            vector<uint16_t> output;
            output.resize(maximumCP - minimumCP, 0);
            if (output.size() & 0x1) {
                output.push_back(0); // pad
            }

            // traverse children recursively (dfs)
            for (const auto& path : paths) {
                // store offsets of the children to the table
                if (path.first >= minimumCP && path.first <= maximumCP) {
                    output[path.first - minimumCP] = path.second.write(out, offset);
                } else {
                    cerr << " ### Encountered distinct code point 0x'" << hex << (int)path.first << " when writing direct array" << endl;
                }
            }
            pos = out.tellp(); // children first
            writePacked(output, out, false);
        }

        // return overall offset
        if ((pos / 2 - offset) > 0x3fff) {
            cerr << " ### Cannot fit offset " << pos << ":" << (pos / 2 - offset) << " into 14 bits, need to redesign !!!!" << endl;
            pos = offset;
        }

        if (endPos) {
            *endPos = out.tellp() / 2;
        }

        return ((pos / 2 - offset) | (uint32_t)type << 14);
    }

    static size_t count;
    static size_t leafCount;
    static uint16_t minimumCP;
    static uint16_t maximumCP;

    uint16_t code { 0 };
    map<uint16_t, Path> paths;
    const vector<uint8_t>* pattern { nullptr };
    bool haveNoncontiguousChildren { false };
};

// declare global data for paths
size_t Path::count;
size_t Path::leafCount;
uint16_t Path::minimumCP = 'j';
uint16_t Path::maximumCP = 'j';

// Struct to hold all the patterns that end with the code.
struct PatternHolder {
    uint16_t code { 0 };
    map<vector<uint16_t>, vector<uint8_t>> patterns;
    map<uint16_t, Path> paths;
};

int main(int argc, char** argv)
{
    if (argc != 2) {
        cout << "usage: './transform hyph-en-us.pat'" << endl;
        return 0;
    }

    ifstream input(argv[1]);
    if (!input.good()) {
        cerr << "could not open '" << argv[1] << "' for reading" << endl;
        return -1;
    }

    string line;
    vector<string> uncategorized;
    map<string, vector<string>> sections;
    vector<string>* current = &uncategorized;
    while (getline(input, line)) {
        string pat;
        if (line.empty()) {
            continue;
        } else if (line[0] == '\\') {
            for (size_t ii = 1; ii < line.size() && !iswspace(line[ii]) && line[ii] != '{'; ii++) {
                pat += line[ii];
            }
            cout << "resolved section " << pat << endl;
            if (!pat.empty()) {
                sections[pat] = vector<string>();
                current = &sections[pat];
            }
            continue;
        } else if (line[0] == '}') {
            current = &uncategorized;
            continue;
        }
        for (auto code : line) {
            if (iswspace(code)) {
                if (!pat.empty()) {
                    current->push_back(pat);
                }
                pat.clear();
                continue;
            }
            if (code == '%') {
                break;
            }
            // cout << code;
            pat += code;
        }
        if (!pat.empty()) {
            current->push_back(pat);
        }
    }

    cout << "Uncategorized data size " << uncategorized.size() << endl;
    cout << "Amount of sections: " << sections.size() << endl;
    for (const auto& section : sections) {
        cout << "  '" << section.first << "' size: " << section.second.size() << endl;
    }

    vector<vector<uint16_t>> utf16_patterns;
    for (auto& pattern : sections["patterns"]) {
        utf16_patterns.push_back(convertToUTF16(pattern));
    }

    // Add exceptions
    for (auto& word : sections["hyphenation"]) {
        // todo, add begin and end markup
        string result;
        bool addedBreak = false;
        for (const auto code : word) {
            if (code == '-') {
                result += to_string(9);
                addedBreak = true;
            } else {
                if (!addedBreak) {
                    result += to_string(8);
                }
                result += code;
                addedBreak = false;
            }
        }
        cout << "Adding exception: " << result << endl;
        utf16_patterns.push_back(convertToUTF16(result));
    }

    map<uint16_t, PatternHolder> leaves;

    // collect leaves and split the rules and patterns to separate arrays
    for (auto& pattern : utf16_patterns) {
        uint16_t ix { 0 };
        size_t endPos = pattern.size();
        for (size_t ii = pattern.size(); ii > 0;) {
            if (!isdigit(pattern[--ii])) {
                ix = pattern[ii];
                break;
            }
        }
        if (ix == 0) {
            continue;
        }

        auto ite = leaves.find(ix);
        if (ite == leaves.end()) {
            leaves[ix] = { PatternHolder() };
        }
        vector<uint16_t> codepoints;
        vector<uint8_t> rules;
        bool addedRule = false;
        for (size_t ii = 0; ii < endPos; ii++) {
            uint16_t code = pattern[ii];
            if (isdigit(code)) {
                rules.push_back(code - '0');
                addedRule = true;
            } else {
                if (!addedRule) {
                    rules.push_back(0);
                }
                codepoints.push_back(code);
                addedRule = false;
            }
        }
        leaves[ix].code = ix;
        if (leaves[ix].patterns.find(codepoints) != leaves[ix].patterns.cend()) {
            cerr << "### Multiple definitions for pattrern with size: " << codepoints.size() << endl;
            cerr << "###";
            for (auto codepoint : codepoints) {
                cerr << " 0x" << hex << (int)codepoint;
            }
            cerr << endl;
        }

        // pad the patterns to the size of four, i.e. 32bits
        while (rules.size() % 4) {
            // primary stategy is to try to strip zeros
            if (rules.back() == 0) {
                rules.pop_back();
            } else {
                break;
            }
        }
        // if not able to align from the back, then add more zeros to pad
        while (rules.size() % 4) {
            // add zeros
            rules.push_back(0);
        }

        leaves[ix].patterns[codepoints] = rules;
    }

    cout << "leaves: " << leaves.size() << endl;
    int countPat = 0;
    bool printCounts = true;
    uint16_t minimumCp {};
    uint16_t maximumCp {};
    // break leave information to Path instances
    for (auto& leave : leaves) {
        cout << "  '" << char(leave.first) << "' rootsize: " << leave.second.patterns.size() << endl;
        for (const auto& pat : leave.second.patterns) {
            if (auto ite = leave.second.paths.find(pat.first[pat.first.size() - 1]); ite != leave.second.paths.end()) {
                ite->second.process(pat.first, pat.first.size() - 1, &pat.second);
            } else {
                leave.second.paths.emplace(pat.first[pat.first.size() - 1], Path(pat.first, &pat.second));
            }
#ifdef VERBOSE_PATTERNS
            cout << "    '";
            for (const auto& digit : pat.first) {
                cout << "'0x" << hex << (int)digit << "' ";
            }
            cout << "' size: " << pat.second.size() << endl;
            cout << "       ";
#endif
            for (const auto& digit : pat.second) {
                (void)digit;
                countPat++;
#ifdef VERBOSE_PATTERNS
                cout << "'" << to_string(digit) << "' ";
            }
            cout << endl;
#else
            }
#endif
        }

        // collect some stats
        for (auto path : leave.second.paths) {
            if (printCounts) {
                cout << "leafs-nodes: " << path.second.leafCount << " / " << path.second.count << endl;
                cout << "min-max: " << path.second.minimumCP << " / " << path.second.maximumCP << endl;
                minimumCp = path.second.minimumCP;
                maximumCp = path.second.maximumCP;
                break;
            }
            path.second.print(10);
        }
    }

    // open output
    string ofName = argv[1];
    ofName += ".hib";
    ofstream out(ofName, ios::binary);

    // very minimalistic magic, perhaps more would be in order including
    // possible version number
    uint32_t header = ('H' | 'H' << 8 | minimumCp << 16 | maximumCp << 24);

    // reserve space for:
    // - header
    // - main toc. and
    // - mapping array for large code points
    // - version
    size_t fulltable = 4;

    for (size_t ii = fulltable; ii != 0; ii--) {
        uint32_t bytes { 0 };
        out.write((const char*)&bytes, sizeof(bytes));
    }

    // Todo: think if is possible to prioritize the paths before writing
    uint32_t offset_table = fulltable * 2;
    struct PathOffset {
        PathOffset(uint32_t o, uint32_t e, uint16_t t, uint16_t c)
            : offset(o)
            , end(e)
            , type(t)
            , code(c)
        {
        }
        int32_t offset;
        int32_t end;
        uint32_t type;
        uint16_t code;
    };

    vector<PathOffset> offsets;
    vector<Path*> bigOnes;
    for (auto& leave : leaves) {
        for (auto& path : leave.second.paths) {
            if (path.first < minimumCp || path.first > maximumCp) {
                bigOnes.push_back(&path.second);
                continue;
            }
            uint16_t end { 0 };
            uint16_t value = path.second.write(out, offset_table, &end, path.first == 'a');
            uint16_t offset = value & 0x3fff;
            uint32_t type = value & 0x0000c000;
            uint16_t code = path.first;
            cout << "direct:" << hex << (int)code << ": " << offset_table << " : " << end << " type " << type << endl;
            offset_table = end;
            offsets.push_back(PathOffset(offset, end, type, code));
        }
    }

    // write distinc code points array after the direct ones
    // ToDo: this loop is same as above so add a util function for this instead copy-paste
    for (auto path : bigOnes) {
        uint16_t end { 0 };
        uint16_t value = path->write(out, offset_table, &end);
        uint16_t offset = value & 0x3fff;
        uint32_t type = value & 0x0000c000;
        uint16_t code = path->code;
        cout << "distinct: 0x" << hex << (int)code << ": " << hex << offset_table << " : " << end << " type " << type << dec << endl;
        offset_table = end;
        offsets.push_back(PathOffset(offset, end, type, code));
    }

    uint32_t toc = out.tellp();
    // and main table offsets
    cout << "Produced " << offsets.size() << " paths with offset: " << toc << endl;
    uint32_t currentEnd = fulltable * 2; // initial offset (in 16 bites)

    auto lastEffectiveIterator = offsets.cbegin();
    Path::writePacked(currentEnd, out);
    vector<uint16_t> mappings;

    if (lastEffectiveIterator != offsets.cend()) {
        // process direct pointing values with holes (to pad the missing entries)
        for (size_t ii = minimumCp; ii <= maximumCp; ii++) {
            auto iterator = offsets.cbegin();
            while (iterator != offsets.cend()) {
                if (iterator->code == ii) {
                    break;
                }
                iterator++;
            }
            if (iterator == offsets.cend()) {
                uint32_t dummy { 0 };
                Path::writePacked(dummy, out);
                Path::writePacked(currentEnd, out);
                cout << "Direct: padded " << endl;
                continue;
            }
            lastEffectiveIterator = iterator;
            uint32_t type = (uint32_t)iterator->type;
            uint32_t bytes = iterator->offset | type << 16;
            currentEnd = iterator->end;
            cout << "Direct: " << hex << "o: 0x" << iterator->offset << " e: 0x" << iterator->end << " t: 0x" << type << " c: 0x" << bytes << endl;
            Path::writePacked(bytes, out);
            Path::writePacked(currentEnd, out);
        }

        // distinct codepoints that cannot be addressed by flat array index
        auto pos = maximumCp + 1;
        while (++lastEffectiveIterator != offsets.cend()) {
            mappings.push_back(lastEffectiveIterator->code);
            mappings.push_back(pos++);
            uint32_t type = (uint32_t)lastEffectiveIterator->type;
            uint32_t bytes = lastEffectiveIterator->offset | type << 16;
            currentEnd = lastEffectiveIterator->end;
            cout << "Distinct: " << hex << "code: 0x" << (int)lastEffectiveIterator->code << " o: 0x" << lastEffectiveIterator->offset << " e: 0x" << lastEffectiveIterator->end << " t: " << type << " c: 0x" << bytes << endl;
            Path::writePacked(bytes, out);
            Path::writePacked(currentEnd, out);
        }
    }
    uint32_t mappingsPos = out.tellp();

    if (!mappings.empty()) {
        Path::writePacked(mappings, out);
    } else {
        uint32_t dummy { 0 };
        Path::writePacked(dummy, out);
    }

    out.seekp(ios::beg); // roll back to the beginning
    if (!out.good()) {
        cerr << "failed to write toc" << endl;
        return -1;
    }

    // write header
    out.write((const char*)&header, sizeof(header));
    // write toc
    out.write((const char*)&toc, sizeof(toc));
    // write mappings
    out.write((const char*)&mappingsPos, sizeof(mappingsPos));
    // write binary version
    const uint32_t version = 0x1 << 24;
    out.write((const char*)&version, sizeof(version));

    cout << "DONE: With " << to_string(countPat) << "patterns (8bit)" << endl;

    input.close();
    out.close();
    return 0;
}
