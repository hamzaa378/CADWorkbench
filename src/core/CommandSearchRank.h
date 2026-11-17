#pragma once
// Portable command relevance scoring. A query's every term must match the
// command label, menu path, or specific common-language alias. Ranking is
// stable; this does not execute commands or alter document selection.
#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace cad::commandsearch {
inline std::string lower(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (unsigned char c:text) result.push_back(static_cast<char>(std::tolower(c)));
    return result;
}
inline std::vector<std::string> words(std::string_view text) {
    std::vector<std::string> result;
    std::string word;
    for (unsigned char c:text) {
        if (std::isalnum(c)) word.push_back(static_cast<char>(std::tolower(c)));
        else if (!word.empty()) {result.push_back(word);word.clear();}
    }
    if (!word.empty()) result.push_back(word);
    return result;
}
inline std::string hints(std::string_view label,std::string_view path) {
    const auto name=lower(label), category=lower(path);
    std::string result;
    const auto contains=[&](std::string_view s){return name.find(s)!=std::string::npos;};
    const auto add=[&](std::string_view s){result.push_back(' ');result.append(s);};
    if (contains("through-hole")||contains("clearance hole")) add("drilling drill fastener bolt screw mounting pass through");
    if (contains("iso 273")) add("m2 m2.5 m3 m4 m5 m6 m8 m10 m12 m14 m16 m20 standard clearance sizing");
    if (contains("blind hole")) add("drill drilling depth bore");
    if (contains("bearing")) add("bush bushing journal shaft roller ball");
    if (contains("washer")) add("spacer ring shim fastener");
    if (contains("nut")) add("threaded fastener hex bolt");
    if (contains("bolt")||contains("screw")) add("fastener threaded screw bolt");
    if (contains("gear")) add("transmission tooth teeth spur drive");
    if (contains("pulley")) add("belt drive transmission wheel");
    if (contains("spring")) add("coil compression tension helix");
    if (contains("flange")) add("pipe connector plate mounting");
    if (contains("sheet-metal")||contains("sheet metal")) add("bend fabrication flat pattern unfold");
    if (contains("dxf")) add("drawing fabrication cnc export");
    if (contains("bom")||contains("parts list")) add("bill of materials manufacturing procurement");
    if (contains("fillet")) add("round radius edge rounding");
    if (contains("chamfer")) add("bevel deburr edge");
    if (contains("constraint")||contains("mate")) add("relationship alignment alignment assemble");
    if (category.find("mechanical")!=std::string::npos) add("machine component");
    return result;
}
inline int score(std::string_view label,std::string_view path,std::string_view query) {
    const auto q=lower(query);
    const auto terms=words(q);
    if (terms.empty()) return 1;
    const auto name=lower(label), location=lower(path), alias=hints(label,path);
    const auto nameWords=words(name), pathWords=words(location), aliasWords=words(alias);
    int total=0;
    for (const auto& term:terms) {
        int best=0;
        if (name==term) best=140;
        else if (name.rfind(term,0)==0) best=85;
        else if (name.find(term)!=std::string::npos) best=58;
        for (const auto& w:nameWords) if (w==term) best=std::max(best,75);
        for (const auto& w:nameWords) if (w.rfind(term,0)==0) best=std::max(best,43);
        for (const auto& w:pathWords) if (w==term) best=std::max(best,25);
        for (const auto& w:pathWords) if (w.rfind(term,0)==0) best=std::max(best,14);
        for (const auto& w:aliasWords) if (w==term) best=std::max(best,30);
        for (const auto& w:aliasWords) if (w.rfind(term,0)==0) best=std::max(best,12);
        if (best==0) return 0;
        total+=best;
    }
    if (name.find(q)!=std::string::npos) total+=65;
    if (name==q) total+=300;
    return total;
}
} // namespace cad::commandsearch
