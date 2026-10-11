#include "NativeSupport.h"
#include <cctype>
namespace entropy::native {
namespace {
bool xmlCharacter(uint32_t cp){return cp==9||cp==10||cp==13||(cp>=32&&cp<=0xd7ff)||(cp>=0xe000&&cp<=0xfffd)||(cp>=0x10000&&cp<=0x10ffff);}
bool validUtf8(std::string_view s){
 for(size_t i=0;i<s.size();){uint32_t cp=(uint8_t)s[i++];int bytes=0;uint32_t minimum=0;
  if(cp<128){if(!xmlCharacter(cp))return false;continue;}
  if(cp>=0xc2&&cp<=0xdf){cp&=31;bytes=1;minimum=128;}else if(cp>=0xe0&&cp<=0xef){cp&=15;bytes=2;minimum=2048;}else if(cp>=0xf0&&cp<=0xf4){cp&=7;bytes=3;minimum=65536;}else return false;
  for(int n=0;n<bytes;++n){if(i>=s.size())return false;auto next=(uint8_t)s[i++];if((next&0xc0)!=0x80)return false;cp=(cp<<6)|(next&63);}
  if(cp<minimum||!xmlCharacter(cp))return false;
 }
 return true;
}
std::string escape(std::string_view s){std::string out;for(char c:s)switch(c){case '&':out+="&amp;";break;case '<':out+="&lt;";break;case '>':out+="&gt;";break;case '\"':out+="&quot;";break;case '\'':out+="&apos;";break;default:out+=c;}return out;}
struct Parser {
 std::string_view s;size_t pos=0;int nodes=0;
 bool starts(std::string_view v)const{return s.substr(pos,v.size())==v;}void need(bool b){if(!b)throw std::invalid_argument("malformed XML");}
 void whitespace(){while(pos<s.size()&&std::isspace((unsigned char)s[pos]))++pos;}
 void trivia(){for(;;){whitespace();if(starts("<?")){auto end=s.find("?>",pos+2);need(end!=s.npos);pos=end+2;}else if(starts("<!--")){auto end=s.find("-->",pos+4);need(end!=s.npos);pos=end+3;}else break;}}
 std::string name(){size_t begin=pos;need(pos<s.size()&&(std::isalpha((unsigned char)s[pos])||s[pos]=='_'||s[pos]==':'));++pos;while(pos<s.size()&&(std::isalnum((unsigned char)s[pos])||s[pos]=='_'||s[pos]==':'||s[pos]=='-'||s[pos]=='.'))++pos;return std::string(s.substr(begin,pos-begin));}
 std::string attribute(){need(pos<s.size()&&(s[pos]=='\''||s[pos]=='\"'));char quote=s[pos++];std::string out;while(pos<s.size()&&s[pos]!=quote){char c=s[pos++];need(c!='<'&&c!='\0');if(c!='&'){out+=c;continue;}auto end=s.find(';',pos);need(end!=s.npos&&end-pos<16);auto e=s.substr(pos,end-pos);pos=end+1;
 if(e=="amp")out+='&';else if(e=="lt")out+='<';else if(e=="gt")out+='>';else if(e=="quot")out+='\"';else if(e=="apos")out+='\'';else if(e.size()>1&&e[0]=='#'){uint32_t cp=0;auto numeric=e.substr(1);int base=10;if(numeric.size()>1&&(numeric[0]=='x'||numeric[0]=='X')){base=16;numeric.remove_prefix(1);}auto r=std::from_chars(numeric.data(),numeric.data()+numeric.size(),cp,base);need(r.ec==std::errc{}&&r.ptr==numeric.data()+numeric.size()&&xmlCharacter(cp));if(cp<128)out+=(char)cp;else if(cp<2048){out+=(char)(0xc0|(cp>>6));out+=(char)(0x80|(cp&63));}else if(cp<65536){out+=(char)(0xe0|(cp>>12));out+=(char)(0x80|((cp>>6)&63));out+=(char)(0x80|(cp&63));}else{out+=(char)(0xf0|(cp>>18));out+=(char)(0x80|((cp>>12)&63));out+=(char)(0x80|((cp>>6)&63));out+=(char)(0x80|(cp&63));}}else need(false);
 }need(pos<s.size());++pos;return out;}
 XmlNode node(int depth){need(depth<64&&++nodes<100000);need(starts("<"));++pos;auto type=name();XmlNode out(type);for(;;){const auto separator=pos;whitespace();if(starts("/>")){pos+=2;return out;}if(starts(">")){++pos;break;}need(pos>separator);auto key=name();need(!out.hasProperty(key));whitespace();need(starts("="));++pos;whitespace();out.setProperty(key,String(attribute()),nullptr);}
 for(;;){trivia();if(starts("</")){pos+=2;need(name()==type);whitespace();need(starts(">"));++pos;return out;}need(starts("<"));out.addChild(node(depth+1),-1,nullptr);}
 }
};
}
std::string XmlNode::serialize()const{if(!d)return {};std::string out="<"+d->type.toStdString();for(auto&[k,v]:d->attrs)out+=" "+k.toStdString()+"=\""+escape(v.toString().toStdString())+"\"";if(d->children.empty())return out+"/>";out+='>';for(auto&c:d->children)out+=c.serialize();return out+"</"+d->type.toStdString()+">";}
std::unique_ptr<XmlNode>XmlNode::parse(std::string_view s){try{if(!validUtf8(s))return {};Parser p{s};p.trivia();auto root=p.node(0);p.trivia();p.need(p.pos==s.size());return std::make_unique<XmlNode>(std::move(root));}catch(...){return {};}}
void copyXmlToBinary(const XmlNode&node,MemoryBlock&block){auto xml=node.serialize();block.setSize(xml.size()+9);auto*p=(uint8_t*)block.getData();uint32_t fields[]={0x21324356,(uint32_t)xml.size()};for(int f=0;f<2;++f)for(int i=0;i<4;++i)p[f*4+i]=(uint8_t)(fields[f]>>(8*i));std::memcpy(p+8,xml.data(),xml.size());p[8+xml.size()]=0;}
std::unique_ptr<XmlNode>getXmlFromBinary(const void*data,int size){if(!data||size<=8||size>64*1024*1024)return {};const auto*p=(const uint8_t*)data;auto u32=[&](int at){return uint32_t(p[at])|(uint32_t(p[at+1])<<8)|(uint32_t(p[at+2])<<16)|(uint32_t(p[at+3])<<24);};auto len=u32(4);if(u32(0)!=0x21324356||len==0||len>uint32_t(size-8))return {};return XmlNode::parse({(const char*)p+8,len});}
}

