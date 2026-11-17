#include "core/DesignBrowserQuery.h"
#include <iostream>
#include <stdexcept>
using namespace cad::browserquery;
static int checks=0;
void expect(bool p,const char* text){++checks;if(!p)throw std::runtime_error(text);}
int main(){
    try{
        BodyIndex b{"Bearing Housing","BRG-204","Stainless Steel 304","abc-1","fillet chamfer pocket",true,false,false};
        expect(matches(b,""),"empty query");
        expect(matches(b,"housing steel"),"free tokens AND search");
        expect(matches(b,"name:\"Bearing Housing\" material:304"),"quoted structured search");
        expect(matches(b,"part:BRG kind:solid state:visible"),"part kind state");
        expect(matches(b,"feature:pocket has:features"),"feature tokens");
        expect(!matches(b,"kind:surface"),"surface guard");
        expect(!matches(b,"state:hidden"),"hidden guard");
        expect(!matches(b,"state:stale"),"stale guard");
        expect(!matches(b,"material:"),"empty field fails closed");
        expect(!matches(b,"unknown:anything"),"unknown field fails closed");
        expect(!matches(b,"material:aluminium"),"material mismatch");
        b.visible=false;b.surface=true;b.stale=true;
        expect(matches(b,"kind:surface state:hidden state:stale"),"combined state filter");
        expect(!matches(b,"state:visible"),"hidden excludes visible");
        expect(!matches(b,"has:missing"),"invalid has tag");
        expect(!matches(b,"foo:bar"),"unknown field remains failure");
        expect(!matches(b,"name:\"unfinished phrase"),"unbalanced quote must fail closed");
        expect(!matches(b,std::string(160,'a')),"long absent query");
        std::cout<<"PASS: "<<checks<<" design browser query assertions\n";
        return 0;
    }catch(const std::exception& ex){std::cerr<<"FAIL: "<<ex.what()<<"\\n";return 1;}
}
