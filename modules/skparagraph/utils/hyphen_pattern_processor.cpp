/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "hyphen_pattern.h"

#include <fstream>
#include <iostream>
#include <map>
#include <unicode/utf.h>
#include <unicode/utf8.h>

namespace OHOS::Hyphenate {
// upper limit for direct pointing arrays
#define MAXIMUM_DIRECT_CODE_POINT 0x7a


vector<uint16_t> ConvertToUtf16(const string& utf8Str)
{
    int32_t i = 0;
    uint32_t c = 0;
    vector<uint16_t> target;
    const int32_t textLength = utf8Str.size();

    while (i < textLength) {
        U8_NEXT(utf8Str.c_str(), i, textLength, c);
        if (U16_LENGTH(c) == 1) {
            target.push_back(c);
        } else {
            target.push_back(U16_LEAD(c));
            target.push_back(U16_TRAIL(c));
        }
    }
    return target;
}

// Recursive path implementation.
// Collects static information and the leafs that provide access to patterns
// The implementation is reversed to pattern code point order; end to beginning of pattern;
struct Path {
    explicit Path(const vector<uint16_t>& path, const vector<uint8_t>* pat)
    {
        count++;
        size_t pathSize = path.size();
        if (pathSize > 0) {
            code = path[--pathSize];
        }

        // Process children recursively
        if (pathSize > 0) {
            Process(path, pathSize, pat);
        } else {
            // store pattern to leafs
            pattern = pat;
            // this is not very clear division yet, but generally
            // the direct array size needs to be limited
            if ((code <= MAXIMUM_DIRECT_CODE_POINT) && (code != '.') && (code != '\'')) {
                maximumCP = max(maximumCP, code);
                minimumCP = min(minimumCP, code);
            }
            leafCount++;
        }
    }

    void Process(const vector<uint16_t>& path, size_t pathSize, const vector<uint8_t>* pat)
    {
        if (pathSize <= 0) {
            return;
        }

        uint16_t key = path[--pathSize];
        if (auto ite = paths.find(key); ite != paths.end()) {
            ite->second.Process(path, pathSize, pat);
        } else {
            if (key > MAXIMUM_DIRECT_CODE_POINT || key == '.' || key == '\'') {
                // if we have direct children with distinct code points, we need to use
                // value pairs
                haveNoncontiguousChildren = true;
            }
            vector<uint16_t> substr(path.cbegin(), path.cbegin() + pathSize + 1);
            // recurse
            paths.emplace(key, Path(substr, pat));
        }
    }

    // Once this node is reached, we can access pattern
    // instead traversing further
    bool IsLeaf() const { return pattern != nullptr; }

    // This instance of Path and its children implement a straight path without ambquity
    // No need to traverse through tables to reach pattern.
    // Calculate the depth of the graph.
    bool IsLinear(size_t& count) const
    {
        count++;
        if (paths.size() == 0) {
            return true;
        } else if (paths.size() == 1) {
            return paths.begin()->second.IsLinear(count);
        }
        return false;
    }

    // debug print misc info
    void Print(size_t indent) const
    {
#ifdef VERBOSE_PATTERNS
        indent += HYPHEN_INDENT_INCREMENT;
        for (size_t i = 0; i < indent; i++) {
            cout << " ";
        }
        if (indent == ROOT_INDENT) {
            cout << char(code) << "rootsize***: " << paths.size();
        } else {
            cout << char(code) << "***: " << paths.size();
        }
        size_t dummy{0};
        if (paths.size() >= LARGE_PATH_SIZE)
            cout << " ###";
        else if (IsLinear(dummy)) {
            cout << " !!!";
        } else {
            cout << " @@@";
        }

        cout << endl;
        if (paths.size() == 0) {
            return;
        }
        for (auto path : paths) {
            path.second.Print(indent);
        }
        cout << endl;
#endif
    }

    static void WritePacked(vector<uint16_t>& data, ostream& out, bool writeCount = true)
    {
        uint16_t size = data.size();
        if (writeCount) {
            out.write(reinterpret_cast<const char*>(&size), sizeof(size));
        }

        if (data.size() & 0x1) {
            data.push_back(0);
        }

        for (size_t i = 0; i < data.size() / HYPHEN_BASE_CODE_SHIFT; i++) {
            uint32_t bytes = data[i * HYPHEN_BASE_CODE_SHIFT] | (data[i * HYPHEN_BASE_CODE_SHIFT + 1] << SHIFT_BITS_16);
            out.write(reinterpret_cast<const char*>(&bytes), sizeof(bytes));
        }
    }

    static void WritePacked(const vector<uint8_t>& data, ostream& out)
    {
        const size_t ALIGN_4BYTES = 0x03;
        size_t size = data.size();
        out.write(reinterpret_cast<const char*>(&size), sizeof(size));

        if (data.size() & ALIGN_4BYTES) {
            cerr << "### uint8_t vectors should be aligned in 4 bytes !!!" << endl;
            size = size & ~ALIGN_4BYTES;
        }

        /* convert uint8 to uint32 */
        for (size_t i = 0; i < size; i += BYTES_PRE_WORD) {
            uint32_t bytes = data[i] | (data[i + 1] << 8) | (data[i + 2] << 16) | (data[i + 3] << 24);
            out.write(reinterpret_cast<const char*>(&bytes), sizeof(bytes));
        }
    }

    // no need to twiddle the bytes or words currently
    static void WritePacked(uint32_t word, ostream& out)
    {
        out.write(reinterpret_cast<const char*>(&word), sizeof(word));
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

    static void WritePackedLine(const Path& pathSrc, ostream& out, PathType& type)
    {
        // this instance is linear, also write pattern
        vector<uint16_t> output;
        const Path* path = &pathSrc;
        auto localPattern = pathSrc.pattern;

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
            WritePacked(output, out);
        } else {
            // otherwise ce can directly jump to pattern
            type = PathType::PATTERN;
            if (output.empty()) {
                output.push_back(0);
            }
            out.write(reinterpret_cast<const char*>(&output[0]), sizeof(output[0]));
        }
        if (localPattern) {
            WritePacked(*localPattern, out);
        } else {
            cerr << "Could not resolve pattern on the linear path !!!" << endl;
        }
    }

    uint16_t write(ostream& out, uint32_t offset = 0, uint16_t* endPos = nullptr, bool breakForCheck = false) const
    {
        if (minimumCP > maximumCP) {
            cerr << "Minimum code point cannot be smaller than maximum, bailing out" << endl;
            return 0;
        }

        PathType type = PathType::DIRECT;

        // kind of laymans conditional breakpoint, could be removed completely
        if (breakForCheck) {
            cout << "### raw 16-bit offset: " << (out.tellp() >> 1) << endl;
        }

        // store the current offset
        uint32_t pos = out.tellp();

        // check if we are linear or should write a table
        size_t count = 0;
        if (IsLinear(count)) {
            WritePackedLine(*this, out, type);
        } else if ((paths.size() < (size_t)(maximumCP - minimumCP) / HYPHEN_BASE_CODE_SHIFT)
                   || haveNoncontiguousChildren) {
            // Using dense table, i.e. value pairs
            // we need to use this also when the code is larger than ff
            vector<uint16_t> output;
            for (const auto& path : paths) {
                output.push_back(path.first);
                output.push_back(path.second.write(out, offset));
            }
            pos = out.tellp(); // our header is after children data
            type = PathType::PAIRS;
            WritePacked(output, out);
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
                    cerr << " ### Encountered distinct code point 0x'" << hex << (int)path.first
                         << " when writing direct array" << endl;
                }
            }
            pos = out.tellp(); // children first
            WritePacked(output, out, false);
        }

        // return overall offset
        if (((pos >> 1) - offset) > 0x3fff) {
            cerr << " ### Cannot fit offset " << pos << ":" << (pos / HYPHEN_BASE_CODE_SHIFT - offset)
                 << " into 14 bits, need to redesign !!!!" << endl;
            pos = offset;
        }

        if (endPos) {
            *endPos = out.tellp() >> 1;
        }

        return (((pos >> 1) - offset) | (static_cast<uint32_t>(type) << SHIFT_BITS_14));
    }

    static size_t count;
    static size_t leafCount;
    static uint16_t minimumCP;
    static uint16_t maximumCP;

    uint16_t code{0};
    map<uint16_t, Path> paths;
    const vector<uint8_t>* pattern{nullptr};
    bool haveNoncontiguousChildren{false};
};

size_t Path::count{0};
size_t Path::leafCount{0};
uint16_t Path::minimumCP = 'j';
uint16_t Path::maximumCP = 'j';

// Struct to hold all the patterns that end with the code.
struct PatternHolder {
    uint16_t code{0};
    map<vector<uint16_t>, vector<uint8_t>> patterns;
    map<uint16_t, Path> paths;
};

struct CpRange {
    uint16_t minimumCp{0};
    uint16_t maximumCp{0};
};

static void ProcessPattern(const string& line, vector<string>* current)
{
    string pat;
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

static void ProcessLine(const string& line, vector<string>* current, vector<string>& uncategorized,
                        map<string, vector<string>>& sections)
{
    string pat;
    if (line.empty()) {
        return;
    } else if (line[0] == '\\') {
        for (size_t i = 1; i < line.size() && !iswspace(line[i]) && line[i] != '{'; i++) {
            pat += line[i];
        }
        cout << "resolved section " << pat << endl;
        if (!pat.empty()) {
            sections[pat] = vector<string>();
            current = &sections[pat];
        }
        return;
    } else if (line[0] == '}') {
        current = &uncategorized;
        return;
    }
    ProcessPattern(line, current);
}

static int32_t ResolveSectionsFromFile(const char* fileName, map<string, vector<string>>& sections)
{
    ifstream input(fileName);
    if (!input.good()) {
        cerr << "could not open '" << fileName << "' for reading" << endl;
        return FAILED;
    }

    string line;
    vector<string> uncategorized;
    vector<string>* current = &uncategorized;
    while (getline(input, line)) {
        ProcessLine(line, current, uncategorized, sections);
    }

    cout << "Uncategorized data size " << uncategorized.size() << endl;
    cout << "Amount of sections: " << sections.size() << endl;
    for (const auto& section : sections) {
        cout << "  '" << section.first << "' size: " << section.second.size() << endl;
    }
    return SUCCEED;
}

static string ProcessWord(const string& word)
{
    string result;
    bool addedBreak = false;
    for (const auto code : word) {
        if (code == '-') {
            result += to_string(BREAK_FLAG);
            addedBreak = true;
        } else {
            if (!addedBreak) {
                result += to_string(NO_BREAK_FLAG);
            }
            result += code;
            addedBreak = false;
        }
    }
    cout << "Adding exception: " << result << endl;
    return result;
}

static void ResolvePatternsFromSections(map<string, vector<string>>& sections, vector<vector<uint16_t>>& utf16Patterns)
{
    for (const auto& pattern : sections.at("patterns")) {
        utf16Patterns.push_back(ConvertToUtf16(pattern));
    }
    for (const auto& word : sections.at("hyphenation")) {
        utf16Patterns.push_back(ConvertToUtf16(ProcessWord(word)));
    }
}

static void CollectLeaves(const vector<uint16_t>& pattern, uint16_t& ix)
{
    for (size_t i = pattern.size(); i > 0;) {
        if (!isdigit(pattern[--i])) {
            ix = pattern[i];
            break;
        }
    }
}

static void ProcessPattern(const vector<uint16_t>& pattern, vector<uint16_t>& codepoints, vector<uint8_t>& rules)
{
    bool addedRule = false;
    for (size_t i = 0; i < pattern.size(); i++) {
        uint16_t code = pattern[i];
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
}

static void PadRules(vector<uint8_t>& rules)
{
    while (rules.size() % PADDING_SIZE) {
        if (rules.back() == 0) {
            rules.pop_back();
        } else {
            break;
        }
    }
    while (rules.size() % PADDING_SIZE) {
        rules.push_back(0);
    }
}

void ResolveLeavesFromPatterns(const vector<vector<uint16_t>>& utf16Patterns, map<uint16_t, PatternHolder>& leaves)
{
    for (const auto& pattern : utf16Patterns) {
        uint16_t ix{0};
        CollectLeaves(pattern, ix);
        if (ix == 0) {
            continue;
        }

        if (leaves.find(ix) == leaves.end()) {
            leaves[ix] = {PatternHolder()};
        }

        vector<uint16_t> codepoints;
        vector<uint8_t> rules;
        ProcessPattern(pattern, codepoints, rules);

        if (leaves[ix].patterns.find(codepoints) != leaves[ix].patterns.cend()) {
            cerr << "### Multiple definitions for pattern with size: " << codepoints.size() << endl;
            cerr << "###";
            for (auto codepoint : codepoints) {
                cerr << " 0x" << hex << static_cast<int>(codepoint);
            }
            cerr << endl;
        }

        PadRules(rules);
        leaves[ix].patterns[codepoints] = rules;
    }

    cout << "leaves: " << leaves.size() << endl;
}

static void BreakLeavesIntoPaths(map<uint16_t, PatternHolder>& leaves, CpRange& range, int& countPat)
{
    bool printCounts = true;
    // break leave information to Path instances
    for (auto& leave : leaves) {
        cout << "  '" << char(leave.first) << "' rootsize: " << leave.second.patterns.size() << endl;
        for (const auto& pat : leave.second.patterns) {
            if (auto ite = leave.second.paths.find(pat.first[pat.first.size() - 1]); ite != leave.second.paths.end()) {
                ite->second.Process(pat.first, pat.first.size() - 1, &pat.second);
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
                range.minimumCp = path.second.minimumCP;
                range.maximumCp = path.second.maximumCP;
                break;
            }
            path.second.Print(HYPHEN_DEFAULT_INDENT);
        }
    }
}

struct PathOffset {
    PathOffset(uint32_t o, uint32_t e, uint16_t t, uint16_t c) : offset(o), end(e), type(t), code(c) {}
    int32_t offset;
    int32_t end;
    uint32_t type;
    uint16_t code;
};

const size_t FULL_TALBLE = 4;

static uint32_t InitOutFileHead(ofstream& out)
{
    // reserve space for:
    // - header
    // - main toc. and
    // - mapping array for large code points
    // - version
    for (size_t i = FULL_TALBLE; i != 0; i--) {
        uint32_t bytes{0};
        out.write(reinterpret_cast<const char*>(&bytes), sizeof(bytes));
    }
    return FULL_TALBLE * 2; // return 2 multiple talble size, check this number
}

static int32_t FormatOutFileHead(ofstream& out, const uint32_t mappingsPos, CpRange range, const uint32_t toc)
{
    out.seekp(ios::beg); // roll back to the beginning
    if (!out.good()) {
        cerr << "failed to write toc" << endl;
        return FAILED;
    }

    // very minimalistic magic, perhaps more would be in order including
    // possible version number
    uint32_t header = ('H' | ('H' << 8) | (range.minimumCp << 16) | (range.maximumCp << 24));
    // write header
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    // write toc
    out.write(reinterpret_cast<const char*>(&toc), sizeof(toc));
    // write mappings
    out.write(reinterpret_cast<const char*>(&mappingsPos), sizeof(mappingsPos));
    // write binary version
    const uint32_t version = 0x1 << 24;
    out.write(reinterpret_cast<const char*>(&version), sizeof(version));
    return SUCCEED;
}

static void WriteLeavePathsToOutFile(map<uint16_t, PatternHolder>& leaves, CpRange& range, ofstream& out,
                                     uint32_t& tableOffset, vector<PathOffset>& offsets, uint32_t& toc)
{
    vector<Path*> bigOnes;
    for (auto& leave : leaves) {
        for (auto& path : leave.second.paths) {
            if (path.first < range.minimumCp || path.first > range.maximumCp) {
                bigOnes.push_back(&path.second);
                continue;
            }
            uint16_t end{0};
            uint16_t value = path.second.write(out, tableOffset, &end, path.first == 'a');
            uint16_t offset = value & 0x3fff;
            uint32_t type = value & 0x0000c000;
            uint16_t code = path.first;
            cout << "direct:" << hex << (int)code << ": " << tableOffset << " : " << end << " type " << type << endl;
            tableOffset = end;
            offsets.push_back(PathOffset(offset, end, type, code));
        }
    }

    // write distinc code points array after the direct ones
    for (auto path : bigOnes) {
        uint16_t end{0};
        uint16_t value = path->write(out, tableOffset, &end);
        uint16_t offset = value & 0x3fff;
        uint32_t type = value & 0x0000c000;
        uint16_t code = path->code;
        cout << "distinct: 0x" << hex << (int)code << ": " << hex << tableOffset << " : " << end << " type " << type
             << dec << endl;
        tableOffset = end;
        offsets.push_back(PathOffset(offset, end, type, code));
    }

    toc = out.tellp();
    // and main table offsets
    cout << "Produced " << offsets.size() << " paths with offset: " << toc << endl;
}

void ProcessDirectPointingValues(std::ofstream& out, const std::vector<PathOffset>& offsets, uint32_t& currentEnd,
                                 CpRange range)
{
    auto lastEffectiveIterator = offsets.cbegin();
    for (size_t i = range.minimumCp; i <= range.maximumCp; i++) {
        auto iterator = offsets.cbegin();
        while (iterator != offsets.cend()) {
            if (iterator->code == i) {
                break;
            }
            iterator++;
        }
        if (iterator == offsets.cend()) {
            uint32_t dummy{0};
            Path::WritePacked(dummy, out);
            Path::WritePacked(currentEnd, out);
            std::cout << "Direct: padded " << std::endl;
            continue;
        }
        lastEffectiveIterator = iterator;
        uint32_t type = static_cast<uint32_t>(iterator->type);
        uint32_t bytes = iterator->offset | type << 16;
        currentEnd = iterator->end;
        std::cout << "Direct: " << std::hex << "o: 0x" << iterator->offset << " e: 0x" << iterator->end << " t: 0x"
                  << type << " c: 0x" << bytes << std::endl;
        Path::WritePacked(bytes, out);
        Path::WritePacked(currentEnd, out);
    }
}

void ProcessDistinctCodepoints(std::ofstream& out, const std::vector<PathOffset>& offsets,
                               std::vector<uint16_t>& mappings, uint32_t& currentEnd, uint16_t maximumCp)
{
    auto lastEffectiveIterator = offsets.cbegin();
    auto pos = maximumCp + 1;
    while (++lastEffectiveIterator != offsets.cend()) {
        mappings.push_back(lastEffectiveIterator->code);
        mappings.push_back(pos++);
        uint32_t type = static_cast<uint32_t>(lastEffectiveIterator->type);
        uint32_t bytes = lastEffectiveIterator->offset | type << 16;
        currentEnd = lastEffectiveIterator->end;
        std::cout << "Distinct: " << std::hex << "code: 0x" << static_cast<int>(lastEffectiveIterator->code) << " o: 0x"
                  << lastEffectiveIterator->offset << " e: 0x" << lastEffectiveIterator->end << " t: " << type
                  << " c: 0x" << bytes << std::endl;
        Path::WritePacked(bytes, out);
        Path::WritePacked(currentEnd, out);
    }
}

static void WriteOffestsToOutFile(ofstream& out, vector<PathOffset>& offsets, uint32_t& mappingsPos, CpRange& range,
                                  uint32_t currentEnd)
{
    auto lastEffectiveIterator = offsets.cbegin();
    vector<uint16_t> mappings;

    if (lastEffectiveIterator != offsets.cend()) {
        // Process direct pointing values with holes (to pad the missing entries)
        ProcessDirectPointingValues(out, offsets, currentEnd, range);

        // distinct codepoints that cannot be addressed by flat array index
        ProcessDistinctCodepoints(out, offsets, mappings, currentEnd, range.maximumCp);
    }

    mappingsPos = out.tellp();

    if (!mappings.empty()) {
        Path::WritePacked(mappings, out);
    } else {
        uint32_t dummy{0};
        Path::WritePacked(dummy, out);
    }
}
} // namespace OHOS::Hyphenate

int main(int argc, char** argv)
{
    if (argc != 2) { // 2: valid argument number
        cout << "usage: './transform hyph-en-us.pat'" << endl;
        return FAILED;
    }

    map<string, vector<string>> sections;
    if (OHOS::Hyphenate::ResolveSectionsFromFile(argv[1], sections)) {
        return FAILED;
    }

    vector<vector<uint16_t>> utf16Patterns;
    OHOS::Hyphenate::ResolvePatternsFromSections(sections, utf16Patterns);

    map<uint16_t, OHOS::Hyphenate::PatternHolder> leaves;
    ResolveLeavesFromPatterns(utf16Patterns, leaves);

    OHOS::Hyphenate::CpRange range = {0, 0};
    int countPat = 0;
    BreakLeavesIntoPaths(leaves, range, countPat);

    // open output
    string ofName = argv[1];
    ofName += ".hib";
    ofstream out(ofName, ios::binary);
    uint32_t tableOffset = OHOS::Hyphenate::InitOutFileHead(out);
    vector<OHOS::Hyphenate::PathOffset> offsets;
    uint32_t toc = 0;

    WriteLeavePathsToOutFile(leaves, range, out, tableOffset, offsets, toc);
    uint32_t currentEnd = OHOS::Hyphenate::FULL_TALBLE * 2; // initial offset (in 16 bites)
    OHOS::Hyphenate::Path::WritePacked(currentEnd, out);

    uint32_t mappingsPos = 0;
    WriteOffestsToOutFile(out, offsets, mappingsPos, range, currentEnd);
    if (OHOS::Hyphenate::FormatOutFileHead(out, mappingsPos, range, toc) != SUCCEED) {
        cout << "DONE: With " << to_string(countPat) << "patterns (8bit)" << endl;
    }
    return SUCCEED;
}
