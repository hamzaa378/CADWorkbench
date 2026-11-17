#include "ParametricStudy.h"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace cad::study {
namespace {
bool safe(const Quantity& q){
    return std::isfinite(q.value) &&
        std::abs(q.unit.length)<=8 && std::abs(q.unit.force)<=8 &&
        std::abs(q.unit.time)<=8 && std::abs(q.unit.temperature)<=8;
}
Dimension add(Dimension a,Dimension b,int sign){
    return {a.length+sign*b.length,a.force+sign*b.force,
            a.time+sign*b.time,a.temperature+sign*b.temperature};
}
Evaluation fail(const std::string& error){return {false,{},error};}
Evaluation pass(Quantity q){return safe(q)?Evaluation{true,q,{}}:fail("Non-finite result or dimensions beyond supported range.");}
std::map<std::string,Quantity> units(){
    return {{"mm",{1,length}},{"cm",{10,length}},{"m",{1000,length}},
            {"in",{25.4,length}},{"ft",{304.8,length}},
            {"N",{1,force}},{"kN",{1000,force}},
            {"Pa",{0.000001,stress}},{"kPa",{0.001,stress}},
            {"MPa",{1,stress}},{"GPa",{1000,stress}},
            {"s",{1,{0,0,1,0}}},{"min",{60,{0,0,1,0}}},
            {"kg",{0.001,mass}},{"g",{0.000001,mass}},
            {"K",{1,{0,0,0,1}}},
            {"deg",{3.14159265358979323846/180,scalar}},
            {"rad",{1,scalar}}};
}
class Parser {
public:
    Parser(std::string_view text,const Resolver& lookup):text_(text),lookup_(lookup){}
    Evaluation run(){
        if(text_.size()>1024)return fail("Expression exceeds 1024 characters.");
        auto value=expression();
        if(!value.valid)return value;
        skip();if(pos_!=text_.size())return fail("Unexpected text at position "+std::to_string(pos_+1)+".");
        return value;
    }
private:
    void skip(){while(pos_<text_.size()&&std::isspace(static_cast<unsigned char>(text_[pos_])))++pos_;}
    bool consume(char c){skip();if(pos_<text_.size()&&text_[pos_]==c){++pos_;return true;}return false;}
    Evaluation expression(){
        auto left=term();if(!left.valid)return left;
        while(true){
            skip();if(pos_>=text_.size()||(text_[pos_]!='+'&&text_[pos_]!='-'))break;
            const char op=text_[pos_++];auto right=term();if(!right.valid)return right;
            if(left.quantity.unit!=right.quantity.unit)return fail("Cannot add/subtract mismatched units.");
            left=pass({left.quantity.value+(op=='+'?1:-1)*right.quantity.value,left.quantity.unit});
            if(!left.valid)return left;
        }
        return left;
    }
    Evaluation term(){
        auto left=unary();if(!left.valid)return left;
        while(true){
            skip();if(pos_>=text_.size()||(text_[pos_]!='*'&&text_[pos_]!='/'))break;
            const char op=text_[pos_++];auto right=unary();if(!right.valid)return right;
            if(op=='/'&&right.quantity.value==0)return fail("Division by zero.");
            left=pass({op=='*'?left.quantity.value*right.quantity.value:
                      left.quantity.value/right.quantity.value,
                      add(left.quantity.unit,right.quantity.unit,op=='*'?1:-1)});
            if(!left.valid)return left;
        }
        return left;
    }
    Evaluation unary(){
        if(++operations_>512)return fail("Expression exceeds operation limit.");
        if(consume('+'))return unary();
        if(consume('-')){auto result=unary();if(result.valid)result.quantity.value=-result.quantity.value;return result;}
        return primary();
    }
    Evaluation primary(){
        skip();if(pos_>=text_.size())return fail("Missing expression operand.");
        if(consume('(')){
            if(++nesting_>32)return fail("Parentheses nesting exceeds limit.");
            auto v=expression();--nesting_;
            if(!v.valid)return v;
            if(!consume(')'))return fail("Missing closing parenthesis.");
            return v;
        }
        const auto ch=static_cast<unsigned char>(text_[pos_]);
        if(std::isdigit(ch)||ch=='.'){
            // Restrict parsed number to its explicitly bounded expression buffer.
            std::string fragment(text_.substr(pos_));char* end=nullptr;
            const double value=std::strtod(fragment.c_str(),&end);
            const auto consumed=static_cast<std::size_t>(end-fragment.c_str());
            if(consumed==0||!std::isfinite(value))return fail("Invalid numeric literal.");
            pos_+=consumed;
            // CAD notation supports '20 mm' or '20mm' without an explicit '*'.
            const std::size_t save=pos_;skip();
            if(pos_<text_.size()&&std::isalpha(static_cast<unsigned char>(text_[pos_]))){
                const auto start=pos_;
                while(pos_<text_.size()&&(std::isalnum(static_cast<unsigned char>(text_[pos_]))||text_[pos_]=='_'))++pos_;
                const auto name=std::string(text_.substr(start,pos_-start));
                static const auto catalog=units();
                const auto it=catalog.find(name);
                if(it!=catalog.end())return pass({value*it->second.value,it->second.unit});
                pos_=save;
            }else pos_=save;
            return pass({value,scalar});
        }
        if(std::isalpha(ch)||ch=='_'){
            const auto start=pos_;
            while(pos_<text_.size()&&(std::isalnum(static_cast<unsigned char>(text_[pos_]))||text_[pos_]=='_'))++pos_;
            const std::string name(text_.substr(start,pos_-start));
            if(name=="pi")return pass({3.14159265358979323846,scalar});
            static const auto catalog=units();
            if(auto it=catalog.find(name);it!=catalog.end())return pass(it->second);
            return lookup_(name);
        }
        return fail("Unexpected character at position "+std::to_string(pos_+1)+".");
    }
    std::string_view text_;
    Resolver lookup_;
    std::size_t pos_=0;
    int operations_=0,nesting_=0;
};
std::string quoted(std::string field){
    if(!field.empty()&&(field[0]=='='||field[0]=='+'||field[0]=='-'||field[0]=='@'))field.insert(field.begin(),'\'');
    std::string result="\"";
    for(char c:field){if(c=='\"')result+='\"';else if(c=='\r'||c=='\n')result+=' ';else result+=c;}
    return result+'\"';
}
}
Evaluation evaluateExpression(const std::string& expression,const Resolver& resolver){
    return Parser(expression,resolver).run();
}
bool evaluateParameterExpressions(const std::map<std::string,Quantity>& baseline,
    const std::map<std::string,std::string>& edits,
    const std::map<std::string,Dimension>& expected,
    std::map<std::string,Quantity>* result,std::string* error){
    if(!result){if(error)*error="Missing output map.";return false;}
    if(baseline.size()>512||edits.size()>512){if(error)*error="Parameter count exceeds 512.";return false;}
    std::map<std::string,Quantity> resolved=baseline;
    std::set<std::string> visiting,completed;
    std::function<Evaluation(const std::string&)> resolve=[&](const std::string& name)->Evaluation{
        const auto edit=edits.find(name);
        if(edit==edits.end()){
            const auto stored=resolved.find(name);
            return stored!=resolved.end()?pass(stored->second):fail("Unknown parameter: "+name);
        }
        if(!baseline.contains(name))return fail("Edited parameter is not present in the model: "+name);
        if(visiting.contains(name))return fail("Cyclic parameter reference at "+name);
        // An edited parameter is always evaluated before its baseline value is used.
        if(completed.contains(name))return pass(resolved.at(name));
        visiting.insert(name);
        auto value=evaluateExpression(edit->second,resolve);
        visiting.erase(name);
        if(!value.valid)return fail(name+": "+value.error);
        const auto requirement=expected.find(name);
        if(requirement!=expected.end()&&value.quantity.unit!=requirement->second)
            return fail(name+": expression has incompatible dimensions.");
        if(!safe(value.quantity))return fail(name+": numeric overflow.");
        resolved[name]=value.quantity;
        completed.insert(name);
        return value;
    };
    for(const auto& [name,_]:edits){
        const auto v=resolve(name);
        if(!v.valid){if(error)*error=v.error;completed.clear();return false;}
    }
    completed.clear();
    *result=std::move(resolved);
    return true;
}
RebuildPlan makeRebuildPlan(const std::vector<FeatureDependency>& nodes,const std::vector<std::string>& changed){
    RebuildPlan result;
    if(nodes.size()>10000){result.error="Graph exceeds 10,000 nodes.";return result;}
    std::map<std::string,std::vector<std::string>> children;
    std::map<std::string,std::size_t> inDegree;
    for(const auto& n:nodes){
        if(n.id.empty()||inDegree.contains(n.id)){
            result.error="Empty or duplicate feature ID: "+n.id;return result;
        }
        inDegree[n.id]=0;
    }
    for(const auto& n:nodes){
        std::set<std::string> seen;
        for(const auto& id:n.inputs){
            if(!inDegree.contains(id)){result.error="Missing feature dependency: "+id;return result;}
            if(!seen.insert(id).second){result.error="Duplicate feature input: "+id;return result;}
            children[id].push_back(n.id);
            ++inDegree[n.id];
        }
    }
    std::set<std::string> ready;
    for(const auto& [id,degree]:inDegree)if(degree==0)ready.insert(id);
    while(!ready.empty()){
        const auto id=*ready.begin();ready.erase(ready.begin());
        result.allTopological.push_back(id);
        for(const auto& child:children[id]){
            const auto remaining=--inDegree[child];
            if(remaining==0)ready.insert(child);
        }
    }
    if(result.allTopological.size()!=nodes.size()){
        result.error="Feature dependency graph contains a cycle.";return result;
    }
    std::set<std::string> dirty;
    for(const auto& id:changed){
        if(!inDegree.contains(id)){result.error="Unknown changed feature: "+id;return result;}
        dirty.insert(id);
    }
    // Topological order guarantees all dependencies precede their consumers.
    for(const auto& id:result.allTopological){
        if(dirty.contains(id))for(const auto& child:children[id])dirty.insert(child);
        if(dirty.contains(id))result.affected.push_back(id);
    }
    result.valid=true;return result;
}
std::vector<ConfigurationResult> evaluateConfigurations(
    const std::map<std::string,Quantity>& baseline,const std::map<std::string,Dimension>& expected,
    const std::vector<Configuration>& cases){
    std::vector<ConfigurationResult> out;out.reserve(cases.size());
    std::set<std::string> names;
    for(const auto& item:cases){
        ConfigurationResult entry;entry.name=item.name;
        if(item.name.empty()||!names.insert(item.name).second)entry.error="Empty or duplicate configuration name.";
        else entry.valid=evaluateParameterExpressions(baseline,item.parameters,expected,&entry.values,&entry.error);
        out.push_back(std::move(entry));
    }
    return out;
}
bool validateMaterial(const MaterialRecord& m,std::string* error){
    auto failMaterial=[&](const std::string& reason){if(error)*error=reason;return false;};
    if(m.id.empty()||m.name.empty()||m.grade.empty()||m.source.empty()||m.revision.empty())
        return failMaterial("Material identity, grade, provenance and revision are required.");
    for(const auto& s:{m.id,m.name,m.grade,m.source,m.revision})
        if(s.size()>200||s.find_first_of("\r\n")!=std::string::npos)return failMaterial("Invalid material text.");
    if(!std::isfinite(m.densityKgPerM3)||m.densityKgPerM3<=0||m.densityKgPerM3>100000||
       !std::isfinite(m.youngsModulusGPa)||m.youngsModulusGPa<=0||m.youngsModulusGPa>2000||
       !std::isfinite(m.yieldStrengthMPa)||m.yieldStrengthMPa<=0||m.yieldStrengthMPa>100000)
        return failMaterial("Material properties are outside the supported range.");
    return true;
}
std::string materialCatalogCsv(const std::vector<MaterialRecord>& records,std::string* error){
    if(records.size()>512){if(error)*error="Material catalog exceeds 512 entries.";return {};}
    std::set<std::string> seen;
    for(const auto& m:records){
        std::string issue;
        if(!validateMaterial(m,&issue)){if(error)*error="Material "+m.id+": "+issue;return {};}
        if(!seen.insert(m.id).second){if(error)*error="Duplicate material ID: "+m.id;return {};}
    }
    std::string csv="id,name,grade,source,revision,density_kg_m3,youngs_modulus_GPa,yield_strength_MPa\n";
    for(const auto& m:records){
        std::ostringstream row;row.precision(12);
        row<<quoted(m.id)<<','<<quoted(m.name)<<','<<quoted(m.grade)<<','<<quoted(m.source)<<','<<quoted(m.revision)<<','
           <<m.densityKgPerM3<<','<<m.youngsModulusGPa<<','<<m.yieldStrengthMPa<<'\n';
        csv+=row.str();
    }
    return csv;
}
}
