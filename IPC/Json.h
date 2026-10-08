#pragma once
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace entropy::ipc {
// Control/observer threads only. Never create or parse Values on an audio owner.
class Value {
public:
    using Array=std::vector<Value>;
    using Object=std::map<std::string,Value,std::less<>>;
    Value():data_(nullptr){}
    Value(std::nullptr_t):data_(nullptr){}
    Value(bool v):data_(v){}
    Value(double v):data_(v){if(!std::isfinite(v))throw std::invalid_argument("Nonfinite JSON number");}
    Value(int v):Value(double(v)){}
    Value(unsigned v):Value(double(v)){}
    Value(std::string v):data_(std::move(v)){}
    Value(const char* v):Value(std::string(v)){}
    Value(Array v):data_(std::move(v)){}
    Value(Object v):data_(std::move(v)){}
    bool isNull() const noexcept{return std::holds_alternative<std::nullptr_t>(data_);}
    bool isNumber() const noexcept{return std::holds_alternative<double>(data_);}
    bool isString() const noexcept{return std::holds_alternative<std::string>(data_);}
    bool isArray() const noexcept{return std::holds_alternative<Array>(data_);}
    bool isObject() const noexcept{return std::holds_alternative<Object>(data_);}
    bool asBool() const{return std::get<bool>(data_);}
    double asNumber() const{return std::get<double>(data_);}
    const std::string& asString() const{return std::get<std::string>(data_);}
    const Array& asArray() const{return std::get<Array>(data_);}
    Array& asArray(){return std::get<Array>(data_);}
    const Object& asObject() const{return std::get<Object>(data_);}
    Object& asObject(){return std::get<Object>(data_);}
    bool contains(std::string_view key) const{return isObject()&&asObject().contains(key);}
    const Value& at(std::string_view key) const {const auto& o=asObject();auto i=o.find(key);if(i==o.end())throw std::invalid_argument("Missing field: "+std::string(key));return i->second;}
    Value& operator[](std::string key){if(isNull())data_=Object{};return asObject()[std::move(key)];}
    std::uint64_t asU64() const {
        const auto& s=asString();std::uint64_t v=0;auto result=std::from_chars(s.data(),s.data()+s.size(),v);
        if(result.ec!=std::errc{}||result.ptr!=s.data()+s.size())throw std::invalid_argument("Invalid uint64 string");return v;
    }
    unsigned asUnsigned(unsigned maximum=UINT32_MAX) const {
        double n=asNumber();if(n<0||n>maximum||std::floor(n)!=n)throw std::invalid_argument("Invalid unsigned field");return unsigned(n);
    }
    static Value u64(std::uint64_t v){return Value(std::to_string(v));}
private:
    std::variant<std::nullptr_t,bool,double,std::string,Array,Object> data_;
    friend std::string dump(const Value&);
};
namespace json_detail {
inline void appendUtf8(std::string& s,unsigned cp){
    if(cp<=0x7f)s+=char(cp);
    else if(cp<=0x7ff){s+=char(0xc0|(cp>>6));s+=char(0x80|(cp&63));}
    else if(cp<=0xffff){s+=char(0xe0|(cp>>12));s+=char(0x80|((cp>>6)&63));s+=char(0x80|(cp&63));}
    else {s+=char(0xf0|(cp>>18));s+=char(0x80|((cp>>12)&63));s+=char(0x80|((cp>>6)&63));s+=char(0x80|(cp&63));}
}
inline bool validUtf8(std::string_view s){
    for(size_t i=0;i<s.size();){unsigned c=static_cast<unsigned char>(s[i++]);if(c<128)continue;
        unsigned count=0,cp=0,min=0;
        if(c>=0xc2&&c<=0xdf){count=1;cp=c&31;min=128;}
        else if(c>=0xe0&&c<=0xef){count=2;cp=c&15;min=2048;}
        else if(c>=0xf0&&c<=0xf4){count=3;cp=c&7;min=65536;}else return false;
        if(i+count>s.size())return false;
        for(unsigned n=0;n<count;++n){c=static_cast<unsigned char>(s[i++]);if((c&0xc0)!=0x80)return false;cp=(cp<<6)|(c&63);}
        if(cp<min||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff))return false;
    }return true;
}
class Parser {
    std::string_view text_;size_t position_=0,nodes_=0;
    [[noreturn]] void fail() const {throw std::invalid_argument("Invalid JSON at byte "+std::to_string(position_));}
    void space(){while(position_<text_.size()&&(text_[position_]==' '||text_[position_]=='\n'||text_[position_]=='\r'||text_[position_]=='\t'))++position_;}
    bool eat(char c){space();if(position_<text_.size()&&text_[position_]==c){++position_;return true;}return false;}
    unsigned hex(){if(position_+4>text_.size())fail();unsigned v=0;for(int n=0;n<4;++n){char c=text_[position_++];unsigned d=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:99;if(d>15)fail();v=v*16+d;}return v;}
    std::string string(){
        if(!eat('"'))fail();std::string out;
        while(position_<text_.size()){
            unsigned char c=text_[position_++];if(c=='"')return out;if(c<32)fail();
            if(c!='\\'){out+=char(c);continue;}if(position_==text_.size())fail();c=text_[position_++];
            switch(c){case '"':case '\\':case '/':out+=char(c);break;case 'b':out+='\b';break;case 'f':out+='\f';break;case 'n':out+='\n';break;case 'r':out+='\r';break;case 't':out+='\t';break;
                case 'u':{unsigned cp=hex();if(cp>=0xd800&&cp<=0xdbff){if(position_+2>text_.size()||text_[position_]!='\\'||text_[position_+1]!='u')fail();position_+=2;auto low=hex();if(low<0xdc00||low>0xdfff)fail();cp=0x10000+((cp-0xd800)<<10)+(low-0xdc00);}else if(cp>=0xdc00&&cp<=0xdfff)fail();appendUtf8(out,cp);break;}default:fail();}
        }fail();
    }
    Value value(unsigned depth){
        if(depth>=64||++nodes_>1000000)fail();space();if(position_==text_.size())fail();char c=text_[position_];
        if(c=='"')return Value(string());
        if(eat('{')){Value::Object o;if(eat('}'))return Value(std::move(o));do{auto key=string();if(!eat(':'))fail();auto v=value(depth+1);if(!o.emplace(std::move(key),std::move(v)).second)fail();if(eat('}'))return Value(std::move(o));}while(eat(','));fail();}
        if(eat('[')){Value::Array a;if(eat(']'))return Value(std::move(a));do{a.push_back(value(depth+1));if(eat(']'))return Value(std::move(a));}while(eat(','));fail();}
        for(auto token:{std::string_view("true"),std::string_view("false"),std::string_view("null")})if(text_.substr(position_,token.size())==token){position_+=token.size();return token=="null"?Value():Value(token=="true");}
        auto begin=position_;if(c=='-')++position_;if(position_==text_.size())fail();
        if(text_[position_]=='0')++position_;else {if(text_[position_]<'1'||text_[position_]>'9')fail();while(position_<text_.size()&&text_[position_]>='0'&&text_[position_]<='9')++position_;}
        if(position_<text_.size()&&text_[position_]=='.'){++position_;auto digits=position_;while(position_<text_.size()&&text_[position_]>='0'&&text_[position_]<'9'+1)++position_;if(position_==digits)fail();}
        if(position_<text_.size()&&(text_[position_]=='e'||text_[position_]=='E')){++position_;if(position_<text_.size()&&(text_[position_]=='+'||text_[position_]=='-'))++position_;auto digits=position_;while(position_<text_.size()&&text_[position_]>='0'&&text_[position_]<='9')++position_;if(position_==digits)fail();}
        double number=0;auto r=std::from_chars(text_.data()+begin,text_.data()+position_,number);if(r.ec!=std::errc{}||r.ptr!=text_.data()+position_||!std::isfinite(number))fail();return Value(number);
    }
public:
    explicit Parser(std::string_view s):text_(s){if(s.size()>64*1024*1024||!validUtf8(s))throw std::invalid_argument("JSON size/UTF-8 invalid");}
    Value run(){auto out=value(0);space();if(position_!=text_.size())fail();return out;}
};
inline std::string quote(std::string_view s){
    if(!validUtf8(s))throw std::invalid_argument("Invalid UTF-8 string");std::string out="\"";constexpr char hex[]="0123456789abcdef";
    for(unsigned char c:s){if(c=='"'||c=='\\'){out+='\\';out+=char(c);}else if(c<32){out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}else out+=char(c);}out+='"';return out;
}
}
inline Value parse(std::string_view s){return json_detail::Parser(s).run();}
inline std::string dump(const Value& v){
    if(v.isNull())return "null";
    if(std::holds_alternative<bool>(v.data_))return v.asBool()?"true":"false";
    if(v.isNumber()){char b[64];auto r=std::to_chars(b,b+sizeof(b),v.asNumber(),std::chars_format::general,std::numeric_limits<double>::max_digits10);if(r.ec!=std::errc{})throw std::runtime_error("Number serialization failed");return {b,r.ptr};}
    if(v.isString())return json_detail::quote(v.asString());
    std::string out=v.isArray()?"[":"{";bool first=true;
    if(v.isArray())for(const auto& entry:v.asArray()){if(!first)out+=',';first=false;out+=dump(entry);}
    else for(const auto& [key,entry]:v.asObject()){if(!first)out+=',';first=false;out+=json_detail::quote(key)+':'+dump(entry);}
    return out+(v.isArray()?"]":"}");
}
}
