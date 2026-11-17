#pragma once
// Small portable query engine used by the Qt design browser and independent C++ tests.
// Filters are conjunctive; invalid field names never silently match everything.
// Names and searchable terms may contain UTF-8. ASCII keyword matching is folded;
// Unicode case mapping belongs to the Qt presentation adapter.
#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace cad::browserquery {
struct BodyIndex {
    std::string name, partNumber, material, id, features;
    bool visible=true, surface=false, stale=false;
};
inline std::string folded(std::string value) {
    for(char& ch:value) if(static_cast<unsigned char>(ch)<128)
        ch=static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return value;
}
inline std::vector<std::string> tokens(const std::string& query) {
    std::vector<std::string> result;
    std::string word;
    bool quote=false;
    for(char ch:query){
        if(ch=='"'){quote=!quote;continue;}
        if(std::isspace(static_cast<unsigned char>(ch))&&!quote){
            if(!word.empty()){result.push_back(word);word.clear();}
        }else word.push_back(ch);
    }
    if(quote)return {"__invalid_unbalanced_quote__:"};
    if(!word.empty())result.push_back(word);
    return result;
}
inline bool contains(const std::string& haystack,const std::string& needle){
    return folded(haystack).find(folded(needle))!=std::string::npos;
}
inline bool matches(const BodyIndex& body,const std::string& query){
    const auto terms=tokens(query);
    if(terms.size()>32)return false;
    for(const auto& raw:terms){
        if(raw.empty())continue;
        const auto colon=raw.find(':');
        const auto key=colon==std::string::npos?std::string():folded(raw.substr(0,colon));
        const auto value=colon==std::string::npos?raw:raw.substr(colon+1);
        if(colon!=std::string::npos&&value.empty())return false;
        if(key.empty()){
            if(!contains(body.name,raw)&&!contains(body.partNumber,raw)&&
               !contains(body.material,raw)&&!contains(body.id,raw)&&
               !contains(body.features,raw))return false;
        }else if(key=="name"){
            if(!contains(body.name,value))return false;
        }else if(key=="part"){
            if(!contains(body.partNumber,value))return false;
        }else if(key=="material"){
            if(!contains(body.material,value))return false;
        }else if(key=="feature"){
            if(!contains(body.features,value))return false;
        }else if(key=="kind"){
            const auto v=folded(value);
            if(v!="solid"&&v!="surface")return false;
            if(body.surface!=(v=="surface"))return false;
        }else if(key=="state"){
            const auto v=folded(value);
            if(v=="visible"&&!body.visible)return false;
            else if(v=="hidden"&&body.visible)return false;
            else if(v=="stale"&&!body.stale)return false;
            else if(v!="visible"&&v!="hidden"&&v!="stale")return false;
        }else if(key=="has"){
            if(folded(value)!="features"||body.features.empty())return false;
        }else return false;
    }
    return true;
}
} // namespace cad::browserquery
